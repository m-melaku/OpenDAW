#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/** Milestone 0: transport, tempo, and a simple arrangement view.
    Drop audio files onto the window to add them as clips on new tracks.
*/
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    // The Engine must outlive the Edit, so it is declared first
    te::Engine engine { ProjectInfo::projectName };
    std::unique_ptr<te::Edit> edit;

    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, settingsButton { "Audio Settings" };
    juce::Slider tempoSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label tempoLabel { {}, "BPM" }, positionLabel;
    bool isDraggingFiles = false;

    static constexpr int toolbarHeight = 40, trackHeight = 60, trackHeaderWidth = 140;

    void togglePlay();
    void stop();
    void addAudioFile (const juce::File&);
    te::AudioTrack* getEmptyTrack();
    void showAudioSettings();

    juce::Rectangle<int> getArrangementArea() const;
    double getPixelsPerSecond() const;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
