#include "MainComponent.h"

namespace
{
    juce::String formatTime (double seconds)
    {
        const auto millis = juce::roundToInt (seconds * 1000.0);
        return juce::String::formatted ("%02d:%02d.%03d", millis / 60000, (millis / 1000) % 60, millis % 1000);
    }
}

MainComponent::MainComponent()
{
    const auto editFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                              .getChildFile ("OpenDAW")
                              .getChildFile ("Untitled.tracktionedit");

    edit = te::createEmptyEdit (engine, editFile);
    edit->getTransport().addChangeListener (this);

    playButton.onClick = [this] { togglePlay(); };
    stopButton.onClick = [this] { stop(); };
    settingsButton.onClick = [this] { showAudioSettings(); };

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.setValue (edit->tempoSequence.getTempo (0)->getBpm(), juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { edit->tempoSequence.getTempo (0)->setBpm (tempoSlider.getValue()); };

    positionLabel.setFont (juce::FontOptions (18.0f));
    positionLabel.setJustificationType (juce::Justification::centred);

    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &settingsButton,
                                                              &tempoSlider, &tempoLabel, &positionLabel })
        addAndMakeVisible (c);

    setWantsKeyboardFocus (true);
    setSize (1000, 600);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    edit->getTransport().removeChangeListener (this);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f22));

    auto area = getArrangementArea();
    g.setColour (juce::Colour (0xff26282c));
    g.fillRect (area);

    const auto pixelsPerSecond = getPixelsPerSecond();
    const auto timelineX = area.getX() + trackHeaderWidth;
    auto row = area.withHeight (trackHeight);

    for (auto* track : te::getAudioTracks (*edit))
    {
        g.setColour (juce::Colour (0xff3a3d42));
        g.drawHorizontalLine (row.getBottom() - 1, (float) row.getX(), (float) row.getRight());

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawText (track->getName(), row.withWidth (trackHeaderWidth).reduced (8, 0), juce::Justification::centredLeft);

        for (auto* clip : track->getClips())
        {
            const auto range = clip->getEditTimeRange();
            const juce::Rectangle<float> clipBounds ((float) (timelineX + range.getStart().inSeconds() * pixelsPerSecond),
                                                     (float) row.getY() + 4.0f,
                                                     (float) (range.getLength().inSeconds() * pixelsPerSecond),
                                                     (float) trackHeight - 8.0f);

            g.setColour (juce::Colour (0xff4f8cc9));
            g.fillRoundedRectangle (clipBounds, 4.0f);
            g.setColour (juce::Colours::white);
            g.drawText (clip->getName(), clipBounds.reduced (6.0f, 2.0f), juce::Justification::topLeft);
        }

        row.translate (0, trackHeight);
    }

    g.setColour (juce::Colour (0xff3a3d42));
    g.drawVerticalLine (timelineX, (float) area.getY(), (float) area.getBottom());

    const auto playheadX = (float) (timelineX + edit->getTransport().getPosition().inSeconds() * pixelsPerSecond);
    g.setColour (juce::Colour (0xffff5c5c));
    g.drawLine (playheadX, (float) area.getY(), playheadX, (float) area.getBottom(), 1.5f);

    if (te::getAudioTracks (*edit).isEmpty() || isDraggingFiles)
    {
        g.setColour (juce::Colours::white.withAlpha (isDraggingFiles ? 0.9f : 0.4f));
        g.drawText ("Drop audio files here", area, juce::Justification::centred);
    }

    if (isDraggingFiles)
    {
        g.setColour (juce::Colour (0xff4f8cc9));
        g.drawRect (area, 2);
    }
}

void MainComponent::resized()
{
    auto toolbar = getLocalBounds().removeFromTop (toolbarHeight).reduced (6);

    playButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (6);
    stopButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (16);
    tempoLabel.setBounds (toolbar.removeFromLeft (40));
    tempoSlider.setBounds (toolbar.removeFromLeft (140));
    settingsButton.setBounds (toolbar.removeFromRight (120));
    positionLabel.setBounds (toolbar);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlay();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        stop();
        return true;
    }

    return false;
}

//==============================================================================
bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    auto& formats = engine.getAudioFileFormatManager().readFormatManager;

    for (auto& f : files)
        if (formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;

    return false;
}

void MainComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    isDraggingFiles = true;
    repaint();
}

void MainComponent::fileDragExit (const juce::StringArray&)
{
    isDraggingFiles = false;
    repaint();
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    isDraggingFiles = false;

    for (auto& f : files)
        addAudioFile (juce::File (f));

    repaint();
}

//==============================================================================
void MainComponent::togglePlay()
{
    auto& transport = edit->getTransport();

    if (transport.isPlaying())
        transport.stop (false, false);
    else
        transport.play (false);
}

void MainComponent::stop()
{
    auto& transport = edit->getTransport();
    transport.stop (false, false);
    transport.setPosition (te::TimePosition());
}

void MainComponent::addAudioFile (const juce::File& file)
{
    te::AudioFile audioFile (engine, file);

    if (! audioFile.isValid())
        return;

    if (auto* track = getEmptyTrack())
    {
        track->setName (file.getFileNameWithoutExtension());
        track->insertWaveClip (file.getFileNameWithoutExtension(), file,
                               { { te::TimePosition(), te::TimeDuration::fromSeconds (audioFile.getLength()) }, {} },
                               false);
    }
}

te::AudioTrack* MainComponent::getEmptyTrack()
{
    auto tracks = te::getAudioTracks (*edit);

    for (auto* track : tracks)
        if (track->getClips().isEmpty())
            return track;

    edit->ensureNumberOfAudioTracks (tracks.size() + 1);
    return te::getAudioTracks (*edit).getLast();
}

void MainComponent::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Audio Settings";
    o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    o.content.setOwned (new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager,
                                                                0, 512, 1, 512, false, false, true, true));
    o.content->setSize (420, 520);
    o.launchAsync();
}

//==============================================================================
juce::Rectangle<int> MainComponent::getArrangementArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight);
}

double MainComponent::getPixelsPerSecond() const
{
    // Fit the whole Edit (or at least 30 seconds) into the visible width
    const auto visibleSeconds = juce::jmax (30.0, edit->getLength().inSeconds() * 1.1);
    return (getArrangementArea().getWidth() - trackHeaderWidth) / visibleSeconds;
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    playButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
}

void MainComponent::timerCallback()
{
    positionLabel.setText (formatTime (edit->getTransport().getPosition().inSeconds()), juce::dontSendNotification);
    repaint (getArrangementArea());
}
