#include "PianoRoll.h"

namespace
{
    namespace Palette
    {
        const juce::Colour background   { 0xff26282c };
        const juce::Colour blackKeyRow  { 0xff212226 };
        const juce::Colour gridBar      { 0xff4a4d53 };
        const juce::Colour gridBeat     { 0xff383b40 };
        const juce::Colour gridStep     { 0xff2d2f34 };
        const juce::Colour outsideClip  { 0x80000000 };
        const juce::Colour note         { 0xff5bb36b };
        const juce::Colour noteSelected { 0xff9be3a8 };
        const juce::Colour playhead     { 0xffff5c5c };
    }

    te::BeatPosition beats (double b)         { return te::BeatPosition::fromBeats (b); }
    te::BeatDuration beatLength (double b)    { return te::BeatDuration::fromBeats (b); }
}

PianoRoll::PianoRoll (te::Edit& e, te::EditItemID id)
    : edit (e), clipID (id)
{
    setWantsKeyboardFocus (true);

    // Start scrolled to the clip's content, and centred on its notes if it has any
    if (auto* clip = getClip())
    {
        scrollBeats = clip->getOffsetInBeats().inBeats();

        const auto& notes = clip->getSequence().getNotes();

        if (! notes.isEmpty())
        {
            int highest = 0;

            for (auto* n : notes)
                highest = juce::jmax (highest, n->getNoteNumber());

            topNote = juce::jlimit (24, 127, highest + 6);
        }
    }

    startTimerHz (30);
}

PianoRoll::~PianoRoll() = default;

//==============================================================================
te::MidiClip* PianoRoll::getClip() const
{
    return dynamic_cast<te::MidiClip*> (te::findClipForID (edit, clipID));
}

te::MidiNote* PianoRoll::findNote (const juce::ValueTree& state) const
{
    if (auto* clip = getClip(); clip != nullptr && state.isValid())
        return clip->getSequence().getNoteFor (state);

    return nullptr;
}

te::MidiNote* PianoRoll::findNoteAt (juce::Point<float> pos, bool& onRightEdge) const
{
    auto* clip = getClip();

    if (clip == nullptr || pos.x < keyboardWidth)
        return nullptr;

    const auto& notes = clip->getSequence().getNotes();

    for (int i = notes.size(); --i >= 0;)
    {
        auto* note = notes.getUnchecked (i);
        const auto bounds = getNoteBounds (*note);

        if (bounds.contains (pos))
        {
            onRightEdge = pos.x > bounds.getRight() - juce::jmin ((float) edgeGrabWidth, bounds.getWidth() / 3.0f);
            return note;
        }
    }

    return nullptr;
}

double PianoRoll::xToBeat (double x) const    { return scrollBeats + (x - keyboardWidth) / pixelsPerBeat; }
double PianoRoll::beatToX (double beat) const { return keyboardWidth + (beat - scrollBeats) * pixelsPerBeat; }
int PianoRoll::yToPitch (double y) const      { return topNote - (int) std::floor (y / rowHeight); }
double PianoRoll::pitchToY (int pitch) const  { return (topNote - pitch) * (double) rowHeight; }

double PianoRoll::snapBeat (double beat) const
{
    const auto grid = gridSize.inBeats();
    return grid > 0.0 ? std::floor (beat / grid + 0.5) * grid : beat;
}

juce::Rectangle<float> PianoRoll::getNoteBounds (const te::MidiNote& note) const
{
    const auto x1 = beatToX (note.getStartBeat().inBeats());
    const auto x2 = beatToX (note.getEndBeat().inBeats());

    return { (float) x1, (float) pitchToY (note.getNoteNumber()) + 1.0f,
             (float) juce::jmax (3.0, x2 - x1), (float) rowHeight - 2.0f };
}

bool PianoRoll::isBlackKey (int pitch)
{
    return juce::MidiMessage::isMidiNoteBlack (pitch);
}

void PianoRoll::deleteNote (te::MidiNote& note)
{
    if (auto* clip = getClip())
    {
        edit.getUndoManager().beginNewTransaction();
        clip->getSequence().removeNote (note, &edit.getUndoManager());
    }

    selectedNote = {};
    repaint();
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto* clip = getClip();

    if (clip == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawText ("This clip no longer exists", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Pitch rows
    for (int pitch = topNote; pitchToY (pitch) < getHeight() && pitch >= 0; --pitch)
    {
        if (isBlackKey (pitch))
        {
            g.setColour (Palette::blackKeyRow);
            g.fillRect (juce::Rectangle<double> (keyboardWidth, pitchToY (pitch), getWidth(), rowHeight).toFloat());
        }
    }

    // Grid lines
    const auto beatsPerBar = juce::jmax (1, edit.tempoSequence.getTimeSigAt (te::TimePosition()).numerator.get());
    const auto grid = gridSize.inBeats();
    const auto showSteps = grid * pixelsPerBeat > 6.0;
    const auto firstBeat = std::floor (xToBeat (keyboardWidth));
    const auto lastBeat = xToBeat (getWidth());

    for (auto beat = firstBeat; beat <= lastBeat; beat += (showSteps ? grid : 1.0))
    {
        const auto x = (float) beatToX (beat);

        if (x < keyboardWidth)
            continue;

        const auto wholeBeat = std::abs (beat - std::round (beat)) < 1.0e-6;
        const auto isBar = wholeBeat && ((int) std::round (beat)) % beatsPerBar == 0;

        g.setColour (isBar ? Palette::gridBar : wholeBeat ? Palette::gridBeat : Palette::gridStep);
        g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
    }

    // Shade the parts outside the clip's visible range
    const auto contentStart = clip->getOffsetInBeats().inBeats();
    const auto contentEnd = contentStart + clip->getLengthInBeats().inBeats();
    g.setColour (Palette::outsideClip);
    g.fillRect (juce::Rectangle<double>::leftTopRightBottom (keyboardWidth, 0, juce::jmax ((double) keyboardWidth, beatToX (contentStart)), getHeight()).toFloat());
    g.fillRect (juce::Rectangle<double>::leftTopRightBottom (juce::jmax ((double) keyboardWidth, beatToX (contentEnd)), 0, getWidth(), getHeight()).toFloat());

    // Notes
    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (getLocalBounds().withTrimmedLeft (keyboardWidth));

        for (auto* note : clip->getSequence().getNotes())
        {
            const auto bounds = getNoteBounds (*note);
            const auto isSelected = note->state == selectedNote;
            const auto velocityAlpha = 0.45f + 0.55f * (float) note->getVelocity() / 127.0f;

            g.setColour ((isSelected ? Palette::noteSelected : Palette::note).withMultipliedAlpha (velocityAlpha));
            g.fillRoundedRectangle (bounds, 2.0f);
            g.setColour (isSelected ? juce::Colours::white : juce::Colours::black.withAlpha (0.4f));
            g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
        }
    }

    // Playhead
    const auto playheadBeat = clip->getContentBeatAtTime (edit.getTransport().getPosition()).inBeats();
    const auto playheadX = (float) beatToX (playheadBeat);

    if (playheadX >= keyboardWidth && playheadBeat >= contentStart && playheadBeat <= contentEnd)
    {
        g.setColour (Palette::playhead);
        g.drawLine (playheadX, 0.0f, playheadX, (float) getHeight(), 1.5f);
    }

    paintKeyboard (g);
}

void PianoRoll::paintKeyboard (juce::Graphics& g)
{
    for (int pitch = topNote; pitchToY (pitch) < getHeight() && pitch >= 0; --pitch)
    {
        const juce::Rectangle<float> key (0.0f, (float) pitchToY (pitch), (float) keyboardWidth, (float) rowHeight);
        const auto black = isBlackKey (pitch);

        g.setColour (black ? juce::Colour (0xff1a1a1a) : juce::Colour (0xffe8e8e8));
        g.fillRect (black ? key.withWidth (keyboardWidth * 0.65f) : key);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawHorizontalLine ((int) key.getBottom() - 1, 0.0f, (float) keyboardWidth);

        if (pitch % 12 == 0)
        {
            g.setColour (juce::Colours::black);
            g.setFont (10.0f);
            g.drawText (juce::MidiMessage::getMidiNoteName (pitch, true, true, 4), key.reduced (2.0f, 0.0f),
                        juce::Justification::centredRight);
        }
    }

    g.setColour (juce::Colours::black);
    g.drawVerticalLine (keyboardWidth - 1, 0.0f, (float) getHeight());
}

//==============================================================================
void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    drag = {};

    auto* clip = getClip();

    if (clip == nullptr || e.position.x < keyboardWidth)
        return;

    bool onRightEdge = false;
    auto* note = findNoteAt (e.position, onRightEdge);

    if (e.mods.isPopupMenu())
    {
        if (note != nullptr)
            deleteNote (*note);

        return;
    }

    edit.getUndoManager().beginNewTransaction();

    if (note == nullptr)
    {
        // Draw a new note, then let the drag set its length
        const auto pitch = juce::jlimit (0, 127, yToPitch (e.position.y));
        const auto start = juce::jmax (0.0, std::floor (xToBeat (e.position.x) / gridSize.inBeats()) * gridSize.inBeats());

        note = clip->getSequence().addNote (pitch, beats (start), lastNoteLength, 100, 0, &edit.getUndoManager());
        onRightEdge = true;
    }

    selectedNote = note->state;

    drag.mode = onRightEdge ? DragMode::resize : DragMode::move;
    drag.note = note->state;
    drag.mouseDownBeat = xToBeat (e.position.x);
    drag.mouseDownPitch = yToPitch (e.position.y);
    drag.originalStart = note->getStartBeat().inBeats();
    drag.originalLength = note->getLengthBeats().inBeats();
    drag.originalPitch = note->getNoteNumber();

    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    auto* note = findNote (drag.note);

    if (note == nullptr || drag.mode == DragMode::none)
        return;

    auto* um = &edit.getUndoManager();
    const auto deltaBeats = xToBeat (e.position.x) - drag.mouseDownBeat;

    if (drag.mode == DragMode::move)
    {
        const auto newStart = juce::jmax (0.0, snapBeat (drag.originalStart + deltaBeats));
        const auto newPitch = juce::jlimit (0, 127, drag.originalPitch + yToPitch (e.position.y) - drag.mouseDownPitch);

        note->setStartAndLength (beats (newStart), beatLength (drag.originalLength), um);

        if (newPitch != note->getNoteNumber())
            note->setNoteNumber (newPitch, um);
    }
    else
    {
        const auto end = snapBeat (drag.originalStart + drag.originalLength + deltaBeats);
        const auto newLength = juce::jmax (gridSize.inBeats(), end - drag.originalStart);

        note->setStartAndLength (beats (drag.originalStart), beatLength (newLength), um);
    }

    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    // New notes default to the length of the last note you drew or resized
    if (drag.mode == DragMode::resize)
        if (auto* note = findNote (drag.note))
            lastNoteLength = note->getLengthBeats();

    drag = {};
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    bool onRightEdge = false;

    if (findNoteAt (e.position, onRightEdge) == nullptr)
        setMouseCursor (juce::MouseCursor::NormalCursor);
    else
        setMouseCursor (onRightEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    bool onRightEdge = false;

    if (auto* note = findNoteAt (e.position, onRightEdge))
        deleteNote (*note);
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        const auto beatAtMouse = xToBeat (e.position.x);
        pixelsPerBeat = juce::jlimit (8.0, 600.0, pixelsPerBeat * (wheel.deltaY > 0 ? 1.2 : 1.0 / 1.2));
        scrollBeats = juce::jmax (0.0, beatAtMouse - (e.position.x - keyboardWidth) / pixelsPerBeat);
    }
    else if (e.mods.isShiftDown() || wheel.deltaX != 0.0f)
    {
        const auto delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        scrollBeats = juce::jmax (0.0, scrollBeats - delta * 300.0 / pixelsPerBeat);
    }
    else
    {
        const auto visibleRows = getHeight() / rowHeight;
        topNote = juce::jlimit (visibleRows - 1, 127, topNote + juce::roundToInt (wheel.deltaY * 12.0f));
    }

    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (auto* note = findNote (selectedNote))
            deleteNote (*note);

        return true;
    }

    return false;
}

void PianoRoll::timerCallback()
{
    if (edit.getTransport().isPlaying())
        repaint();
}
