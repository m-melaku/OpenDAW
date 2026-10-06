#include "ArrangementView.h"

namespace
{
    namespace Palette
    {
        const juce::Colour background   { 0xff26282c };
        const juce::Colour header       { 0xff2e3035 };
        const juce::Colour ruler        { 0xff1e1f22 };
        const juce::Colour gridBar      { 0xff4a4d53 };
        const juce::Colour gridBeat     { 0xff33363b };
        const juce::Colour separator    { 0xff3a3d42 };
        const juce::Colour clip         { 0xff3d6f9e };
        const juce::Colour clipSelected { 0xff5b9be0 };
        const juce::Colour midiClip     { 0xff3e7f4a };
        const juce::Colour midiClipSelected { 0xff58b068 };
        const juce::Colour waveform     { 0xffd6e6f5 };
        const juce::Colour playhead     { 0xffff5c5c };
        const juce::Colour mute         { 0xffe0b23a };
        const juce::Colour solo         { 0xff4fc3a1 };
    }

    te::TimePosition seconds (double s)     { return te::TimePosition::fromSeconds (s); }

    te::AudioFile makeAudioFile (te::Engine& engine, const juce::String& path)
    {
        return te::AudioFile (engine, juce::File (path));
    }
}

ArrangementView::ArrangementView (te::Edit& e)
    : edit (e)
{
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

ArrangementView::~ArrangementView() = default;

//==============================================================================
void ArrangementView::addTrack()
{
    edit.getUndoManager().beginNewTransaction();
    edit.ensureNumberOfAudioTracks (getTracks().size() + 1);
    repaint();
}

void ArrangementView::addInstrumentTrack()
{
    edit.getUndoManager().beginNewTransaction();
    edit.ensureNumberOfAudioTracks (getTracks().size() + 1);

    auto& track = *getTracks().getLast();
    track.setName ("Instrument " + juce::String (getTracks().size()));
    ensureInstrument (track);
    repaint();
}

void ArrangementView::ensureInstrument (te::AudioTrack& track)
{
    for (auto* plugin : track.pluginList.getPlugins())
        if (plugin->isSynth())
            return;

    // Every MIDI track needs something to make sound, so default to the built-in 4OSC synth
    if (auto synth = edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
        track.pluginList.insertPlugin (synth, 0, nullptr);
}

void ArrangementView::deleteSelectedClip()
{
    if (auto* clip = te::findClipForID (edit, selectedClipID))
    {
        edit.getUndoManager().beginNewTransaction();
        clip->removeFromParent();
    }

    selectedClipID = {};
    repaint();
}

bool ArrangementView::hasSelectedClip() const
{
    return te::findClipForID (edit, selectedClipID) != nullptr;
}

//==============================================================================
double ArrangementView::timeToX (double s) const    { return headerWidth + (s - scrollSeconds) * pixelsPerSecond; }
double ArrangementView::xToTime (double x) const    { return scrollSeconds + (x - headerWidth) / pixelsPerSecond; }

double ArrangementView::snap (double s) const
{
    if (! snapToGrid)
        return s;

    auto& ts = edit.tempoSequence;
    const auto beat = std::round (ts.toBeats (seconds (s)).inBeats());
    return ts.toTime (te::BeatPosition::fromBeats (beat)).inSeconds();
}

int ArrangementView::trackIndexAtY (int y) const
{
    return (int) std::floor ((y - rulerHeight + scrollY) / (double) trackHeight);
}

int ArrangementView::getTrackY (int index) const
{
    return rulerHeight + index * trackHeight - scrollY;
}

int ArrangementView::getMaxScrollY() const
{
    return juce::jmax (0, (getTracks().size() + 1) * trackHeight - (getHeight() - rulerHeight));
}

juce::Rectangle<float> ArrangementView::getClipBounds (te::Clip& clip, int trackIndex) const
{
    const auto range = clip.getEditTimeRange();
    const auto x1 = timeToX (range.getStart().inSeconds());
    const auto x2 = timeToX (range.getEnd().inSeconds());

    return { (float) x1, (float) getTrackY (trackIndex) + 3.0f,
             (float) juce::jmax (2.0, x2 - x1), (float) trackHeight - 6.0f };
}

te::Clip* ArrangementView::findClipAt (juce::Point<float> pos, DragMode& modeOut) const
{
    if (pos.x < headerWidth || pos.y < rulerHeight)
        return nullptr;

    const auto index = trackIndexAtY ((int) pos.y);
    const auto tracks = getTracks();

    if (! juce::isPositiveAndBelow (index, tracks.size()))
        return nullptr;

    // Search backwards so the clip drawn on top wins
    const auto& clips = tracks[index]->getClips();

    for (int i = clips.size(); --i >= 0;)
    {
        auto* clip = clips.getUnchecked (i);
        const auto bounds = getClipBounds (*clip, index);

        if (bounds.contains (pos))
        {
            const auto grab = juce::jmin ((float) edgeGrabWidth, bounds.getWidth() / 3.0f);

            if (pos.x < bounds.getX() + grab)          modeOut = DragMode::trimStart;
            else if (pos.x > bounds.getRight() - grab) modeOut = DragMode::trimEnd;
            else                                       modeOut = DragMode::move;

            return clip;
        }
    }

    return nullptr;
}

juce::Rectangle<int> ArrangementView::getMuteButtonBounds (int trackIndex) const
{
    return { headerWidth - 52, getTrackY (trackIndex) + 8, 22, 18 };
}

juce::Rectangle<int> ArrangementView::getSoloButtonBounds (int trackIndex) const
{
    return getMuteButtonBounds (trackIndex).translated (24, 0);
}

te::SmartThumbnail& ArrangementView::getThumbnail (te::AudioClipBase& clip)
{
    auto& thumb = thumbnails[clip.itemID.toString()];

    if (thumb == nullptr)
        thumb = std::make_unique<te::SmartThumbnail> (edit.engine, clip.getAudioFile(), *this, &edit);

    return *thumb;
}

te::AudioTrack& ArrangementView::getTrackForDrop (int trackIndex)
{
    auto tracks = getTracks();

    if (juce::isPositiveAndBelow (trackIndex, tracks.size()))
        return *tracks[trackIndex];

    // Dropped below the last track: reuse it if empty, otherwise add a new one
    if (! tracks.isEmpty() && tracks.getLast()->getClips().isEmpty())
        return *tracks.getLast();

    edit.ensureNumberOfAudioTracks (tracks.size() + 1);
    return *getTracks().getLast();
}

void ArrangementView::showTrackMenu (te::AudioTrack& track)
{
    juce::PopupMenu menu;
    menu.addItem ("Delete Track", [this, trackID = track.itemID]
    {
        if (auto* t = te::findTrackForID (edit, trackID))
        {
            edit.getUndoManager().beginNewTransaction();
            edit.deleteTrack (t);
            repaint();
        }
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

//==============================================================================
void ArrangementView::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    const auto tracks = getTracks();

    for (int i = 0; i < tracks.size(); ++i)
        paintTrack (g, *tracks[i], i);

    paintRuler (g);

    for (int i = 0; i < tracks.size(); ++i)
        paintHeader (g, *tracks[i], i);

    g.setColour (Palette::ruler);
    g.fillRect (0, 0, headerWidth, rulerHeight);
    g.setColour (Palette::separator);
    g.drawVerticalLine (headerWidth - 1, 0.0f, (float) getHeight());

    const auto playheadX = (float) timeToX (edit.getTransport().getPosition().inSeconds());

    if (playheadX >= headerWidth)
    {
        g.setColour (Palette::playhead);
        g.drawLine (playheadX, 0.0f, playheadX, (float) getHeight(), 1.5f);
    }

    if (isDraggingFiles)
    {
        g.setColour (Palette::clipSelected);
        g.drawRect (getLocalBounds(), 2);
    }
    else if (tracks.size() <= 1 && (tracks.isEmpty() || tracks[0]->getClips().isEmpty()))
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawText ("Drop audio files here", getLocalBounds().withTrimmedLeft (headerWidth), juce::Justification::centred);
    }
}

void ArrangementView::paintRuler (juce::Graphics& g)
{
    const auto area = getLocalBounds().withTrimmedLeft (headerWidth);
    g.setColour (Palette::ruler);
    g.fillRect (area.withHeight (rulerHeight));

    auto& ts = edit.tempoSequence;
    const auto beatsPerBar = juce::jmax (1, ts.getTimeSigAt (te::TimePosition()).numerator.get());
    const auto firstBeat = (int) std::floor (ts.toBeats (seconds (juce::jmax (0.0, xToTime (headerWidth)))).inBeats());
    const auto lastBeat  = (int) std::ceil  (ts.toBeats (seconds (juce::jmax (0.0, xToTime (getWidth())))).inBeats());

    // Only draw individual beats when they're far enough apart to be readable
    const auto pixelsPerBeat = timeToX (ts.toTime (te::BeatPosition::fromBeats (firstBeat + 1)).inSeconds())
                             - timeToX (ts.toTime (te::BeatPosition::fromBeats (firstBeat)).inSeconds());
    const auto showBeats = pixelsPerBeat > 8.0;
    const auto barLabelStep = juce::jmax (1, (int) std::ceil (40.0 / (pixelsPerBeat * beatsPerBar)));

    g.setFont (12.0f);

    for (int beat = juce::jmax (0, firstBeat); beat <= lastBeat; ++beat)
    {
        const auto isBar = beat % beatsPerBar == 0;

        if (! isBar && ! showBeats)
            continue;

        const auto x = (float) timeToX (ts.toTime (te::BeatPosition::fromBeats (beat)).inSeconds());

        if (x < headerWidth)
            continue;

        g.setColour (isBar ? Palette::gridBar : Palette::gridBeat);
        g.drawVerticalLine ((int) x, (float) rulerHeight, (float) getHeight());

        const auto bar = beat / beatsPerBar;

        if (isBar && bar % barLabelStep == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawVerticalLine ((int) x, rulerHeight * 0.5f, (float) rulerHeight);
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 3.0f, 0.0f, 40.0f, (float) rulerHeight),
                        juce::Justification::centredLeft);
        }
    }

    g.setColour (Palette::separator);
    g.drawHorizontalLine (rulerHeight - 1, (float) headerWidth, (float) getWidth());
}

void ArrangementView::paintTrack (juce::Graphics& g, te::AudioTrack& track, int trackIndex)
{
    const auto y = getTrackY (trackIndex);

    if (y + trackHeight < rulerHeight || y > getHeight())
        return;

    g.setColour (Palette::separator);
    g.drawHorizontalLine (y + trackHeight - 1, (float) headerWidth, (float) getWidth());

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (getLocalBounds().withTrimmedLeft (headerWidth).withTrimmedTop (rulerHeight));

    for (auto* clip : track.getClips())
        paintClip (g, *clip, getClipBounds (*clip, trackIndex));
}

void ArrangementView::paintClip (juce::Graphics& g, te::Clip& clip, juce::Rectangle<float> bounds)
{
    if (bounds.getRight() < headerWidth || bounds.getX() > getWidth())
        return;

    const auto isSelected = clip.itemID == selectedClipID;
    auto* midiClip = dynamic_cast<te::MidiClip*> (&clip);

    if (midiClip != nullptr)
        g.setColour (isSelected ? Palette::midiClipSelected : Palette::midiClip);
    else
        g.setColour (isSelected ? Palette::clipSelected : Palette::clip);

    g.fillRoundedRectangle (bounds, 4.0f);

    if (midiClip != nullptr)
    {
        // Mini piano roll: notes scaled to fit the clip's pitch range
        const auto& notes = midiClip->getSequence().getNotes();

        if (! notes.isEmpty())
        {
            int lowest = 127, highest = 0;

            for (auto* n : notes)
            {
                lowest = juce::jmin (lowest, n->getNoteNumber());
                highest = juce::jmax (highest, n->getNoteNumber());
            }

            const auto noteArea = bounds.reduced (2.0f).withTrimmedTop (14.0f);
            const auto rowHeight = noteArea.getHeight() / (float) juce::jmax (8, highest - lowest + 1);
            const auto offsetBeats = midiClip->getOffsetInBeats().inBeats();
            const auto beatsToPixels = bounds.getWidth() / juce::jmax (0.001, midiClip->getLengthInBeats().inBeats());

            juce::Graphics::ScopedSaveState saveState (g);
            g.reduceClipRegion (noteArea.toNearestInt());
            g.setColour (Palette::waveform.withAlpha (0.9f));

            for (auto* n : notes)
            {
                const auto x = bounds.getX() + (float) ((n->getStartBeat().inBeats() - offsetBeats) * beatsToPixels);
                const auto w = juce::jmax (1.5f, (float) (n->getLengthBeats().inBeats() * beatsToPixels));
                const auto y = noteArea.getBottom() - (float) (n->getNoteNumber() - lowest + 1) * rowHeight;
                g.fillRect (x, y, w, juce::jmax (1.5f, rowHeight - 1.0f));
            }
        }
    }
    else if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (&clip))
    {
        const auto pos = clip.getPosition();
        auto& thumb = getThumbnail (*audioClip);
        const auto waveArea = bounds.reduced (2.0f).withTrimmedTop (14.0f).toNearestInt();

        g.setColour (Palette::waveform.withAlpha (0.85f));
        thumb.drawChannels (g, waveArea, te::TimeRange (te::toPosition (pos.getOffset()), pos.getLength()), 1.0f);
    }

    g.setColour (juce::Colours::white);
    g.setFont (12.0f);
    g.drawText (clip.getName(), bounds.reduced (6.0f, 1.0f).withHeight (14.0f), juce::Justification::centredLeft);

    if (isSelected)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (bounds, 4.0f, 1.5f);
    }
}

void ArrangementView::paintHeader (juce::Graphics& g, te::AudioTrack& track, int trackIndex)
{
    const auto y = getTrackY (trackIndex);

    if (y + trackHeight < rulerHeight || y > getHeight())
        return;

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (0, rulerHeight, headerWidth, getHeight() - rulerHeight);

    const juce::Rectangle<int> area (0, y, headerWidth, trackHeight);
    g.setColour (Palette::header);
    g.fillRect (area);
    g.setColour (Palette::separator);
    g.drawHorizontalLine (area.getBottom() - 1, 0.0f, (float) headerWidth);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (13.0f);
    g.drawText (track.getName(), area.reduced (8, 6).withTrimmedRight (56).withHeight (20), juce::Justification::centredLeft);

    auto drawToggle = [&g] (juce::Rectangle<int> r, const juce::String& text, bool isOn, juce::Colour onColour)
    {
        g.setColour (isOn ? onColour : Palette::separator);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (isOn ? juce::Colours::black : juce::Colours::white.withAlpha (0.7f));
        g.setFont (11.0f);
        g.drawText (text, r, juce::Justification::centred);
    };

    drawToggle (getMuteButtonBounds (trackIndex), "M", track.isMuted (false), Palette::mute);
    drawToggle (getSoloButtonBounds (trackIndex), "S", track.isSolo (false), Palette::solo);
}

//==============================================================================
void ArrangementView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    drag = {};

    const auto pos = e.position;
    const auto tracks = getTracks();

    // Ruler: move the playhead
    if (pos.y < rulerHeight)
    {
        if (pos.x >= headerWidth)
        {
            drag.mode = DragMode::scrub;
            edit.getTransport().setPosition (seconds (juce::jmax (0.0, xToTime (pos.x))));
        }

        return;
    }

    // Track headers: mute/solo and the track menu
    if (pos.x < headerWidth)
    {
        const auto index = trackIndexAtY ((int) pos.y);

        if (! juce::isPositiveAndBelow (index, tracks.size()))
            return;

        auto& track = *tracks[index];

        if (e.mods.isPopupMenu())
        {
            showTrackMenu (track);
        }
        else if (getMuteButtonBounds (index).contains (pos.toInt()))
        {
            edit.getUndoManager().beginNewTransaction();
            track.setMute (! track.isMuted (false));
        }
        else if (getSoloButtonBounds (index).contains (pos.toInt()))
        {
            edit.getUndoManager().beginNewTransaction();
            track.setSolo (! track.isSolo (false));
        }

        repaint();
        return;
    }

    // Clips: select and start a move/trim
    DragMode mode = DragMode::none;

    if (auto* clip = findClipAt (pos, mode))
    {
        selectedClipID = clip->itemID;

        const auto clipPos = clip->getPosition();
        drag.mode = mode;
        drag.clipID = clip->itemID;
        drag.mouseDownTime = xToTime (pos.x);
        drag.originalStart = clipPos.getStart().inSeconds();
        drag.originalEnd = clipPos.getEnd().inSeconds();
        drag.originalOffset = clipPos.getOffset().inSeconds();
        drag.maxEnd = drag.originalStart - drag.originalOffset + clip->getMaximumLength().inSeconds();

        edit.getUndoManager().beginNewTransaction();
    }
    else
    {
        // Empty space: deselect and move the playhead here
        selectedClipID = {};
        edit.getTransport().setPosition (seconds (juce::jmax (0.0, snap (xToTime (pos.x)))));
    }

    repaint();
}

void ArrangementView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag.mode == DragMode::scrub)
    {
        edit.getTransport().setPosition (seconds (juce::jmax (0.0, xToTime (e.position.x))));
        return;
    }

    auto* clip = te::findClipForID (edit, drag.clipID);

    if (clip == nullptr || drag.mode == DragMode::none)
        return;

    const auto delta = xToTime (e.position.x) - drag.mouseDownTime;

    switch (drag.mode)
    {
        case DragMode::move:
        {
            const auto newStart = juce::jmax (0.0, snap (drag.originalStart + delta));
            clip->setStart (seconds (newStart), false, true);

            const auto tracks = getTracks();
            const auto targetIndex = juce::jlimit (0, tracks.size() - 1, trackIndexAtY ((int) e.position.y));

            if (tracks[targetIndex] != clip->getClipTrack())
                clip->moveTo (*tracks[targetIndex]);

            break;
        }

        case DragMode::trimStart:
        {
            // Can't trim back past the start of the source audio
            const auto minStart = juce::jmax (0.0, drag.originalStart - drag.originalOffset);
            const auto newStart = juce::jlimit (minStart, drag.originalEnd - minClipLength, snap (drag.originalStart + delta));
            clip->setStart (seconds (newStart), true, false);
            break;
        }

        case DragMode::trimEnd:
        {
            const auto newEnd = juce::jlimit (drag.originalStart + minClipLength, drag.maxEnd, snap (drag.originalEnd + delta));
            clip->setEnd (seconds (newEnd), true);
            break;
        }

        case DragMode::none:
        case DragMode::scrub:
            break;
    }

    repaint();
}

void ArrangementView::mouseUp (const juce::MouseEvent&)
{
    drag = {};
}

void ArrangementView::mouseMove (const juce::MouseEvent& e)
{
    DragMode mode = DragMode::none;

    if (findClipAt (e.position, mode) == nullptr)
        setMouseCursor (juce::MouseCursor::NormalCursor);
    else if (mode == DragMode::move)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void ArrangementView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.position.x < headerWidth || e.position.y < rulerHeight)
        return;

    DragMode mode = DragMode::none;

    // Double-click a MIDI clip to edit it
    if (auto* clip = findClipAt (e.position, mode))
    {
        if (dynamic_cast<te::MidiClip*> (clip) != nullptr && onOpenMidiClip != nullptr)
            onOpenMidiClip (clip->itemID);

        return;
    }

    // Double-click empty space on a track to create a one-bar MIDI clip there
    const auto tracks = getTracks();
    const auto index = trackIndexAtY ((int) e.position.y);

    if (! juce::isPositiveAndBelow (index, tracks.size()))
        return;

    auto& track = *tracks[index];
    auto& ts = edit.tempoSequence;
    const auto beatsPerBar = juce::jmax (1, ts.getTimeSigAt (te::TimePosition()).numerator.get());
    const auto clickBeat = ts.toBeats (seconds (juce::jmax (0.0, xToTime (e.position.x)))).inBeats();
    const auto startBeat = std::floor (clickBeat / beatsPerBar) * beatsPerBar;

    const te::TimeRange range (ts.toTime (te::BeatPosition::fromBeats (startBeat)),
                               ts.toTime (te::BeatPosition::fromBeats (startBeat + beatsPerBar)));

    edit.getUndoManager().beginNewTransaction();
    ensureInstrument (track);

    if (auto clip = track.insertMIDIClip ("MIDI Clip", range, nullptr))
    {
        selectedClipID = clip->itemID;

        if (onOpenMidiClip != nullptr)
            onOpenMidiClip (clip->itemID);
    }

    repaint();
}

void ArrangementView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        // Zoom around the mouse position
        const auto timeAtMouse = xToTime (e.position.x);
        pixelsPerSecond = juce::jlimit (2.0, 2000.0, pixelsPerSecond * (wheel.deltaY > 0 ? 1.2 : 1.0 / 1.2));
        scrollSeconds = juce::jmax (0.0, timeAtMouse - (e.position.x - headerWidth) / pixelsPerSecond);
    }
    else if (e.mods.isShiftDown() || wheel.deltaX != 0.0f)
    {
        const auto delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        scrollSeconds = juce::jmax (0.0, scrollSeconds - delta * 300.0 / pixelsPerSecond);
    }
    else
    {
        scrollY = juce::jlimit (0, getMaxScrollY(), scrollY - juce::roundToInt (wheel.deltaY * 200.0f));
    }

    repaint();
}

bool ArrangementView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedClip();
        return true;
    }

    return false;
}

//==============================================================================
bool ArrangementView::isInterestedInFileDrag (const juce::StringArray& files)
{
    auto& formats = edit.engine.getAudioFileFormatManager().readFormatManager;

    for (auto& f : files)
        if (formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;

    return false;
}

void ArrangementView::fileDragEnter (const juce::StringArray&, int, int)
{
    isDraggingFiles = true;
    repaint();
}

void ArrangementView::fileDragExit (const juce::StringArray&)
{
    isDraggingFiles = false;
    repaint();
}

void ArrangementView::filesDropped (const juce::StringArray& files, int x, int y)
{
    isDraggingFiles = false;
    edit.getUndoManager().beginNewTransaction();

    const auto startTime = x > headerWidth ? juce::jmax (0.0, snap (xToTime (x))) : 0.0;
    auto trackIndex = y > rulerHeight ? trackIndexAtY (y) : 0;

    for (auto& path : files)
    {
        const auto audioFile = makeAudioFile (edit.engine, path);

        if (! audioFile.isValid())
            continue;

        const juce::File file (path);
        auto& track = getTrackForDrop (trackIndex);

        if (track.getClips().isEmpty())
            track.setName (file.getFileNameWithoutExtension());

        const te::ClipPosition position { { seconds (startTime), te::TimeDuration::fromSeconds (audioFile.getLength()) }, {} };

        if (auto clip = track.insertWaveClip (file.getFileNameWithoutExtension(), file, position, false))
        {
            // Store absolute paths so projects can be saved anywhere without breaking links
            clip->getSourceFileReference().source.setValue (file.getFullPathName(), nullptr);
            selectedClipID = clip->itemID;
        }

        // Each additional file goes on the next track down
        trackIndex = te::getAudioTracks (edit).indexOf (&track) + 1;
    }

    repaint();
}

//==============================================================================
void ArrangementView::timerCallback()
{
    auto& transport = edit.getTransport();

    // Page the view along with the playhead during playback
    if (transport.isPlaying() && drag.mode == DragMode::none)
    {
        const auto playheadX = timeToX (transport.getPosition().inSeconds());

        if (playheadX > getWidth() - 20 || playheadX < headerWidth)
            scrollSeconds = juce::jmax (0.0, transport.getPosition().inSeconds() - 1.0);
    }

    repaint();
}
