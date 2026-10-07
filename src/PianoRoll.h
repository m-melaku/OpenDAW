#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/** A note editor for one MIDI clip.

    - Click empty space to draw a note (drag to set its length)
    - Drag a note to move it, drag its right edge to resize it
    - Right-click or double-click a note to delete it; Delete removes the selection
    - Ctrl+wheel zooms horizontally, wheel scrolls pitches, Shift+wheel scrolls time
*/
class PianoRoll : public juce::Component,
                  private juce::Timer
{
public:
    PianoRoll (te::Edit&, te::EditItemID clipID);
    ~PianoRoll() override;

    te::EditItemID getClipID() const    { return clipID; }

    void setGridSize (te::BeatDuration newSize)     { gridSize = newSize; repaint(); }
    te::BeatDuration getGridSize() const            { return gridSize; }

    //==============================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    te::Edit& edit;
    const te::EditItemID clipID;

    static constexpr int keyboardWidth = 56, rowHeight = 12, edgeGrabWidth = 5;

    te::BeatDuration gridSize = te::BeatDuration::fromBeats (0.25);
    te::BeatDuration lastNoteLength = te::BeatDuration::fromBeats (1.0);
    double pixelsPerBeat = 60.0;
    double scrollBeats = 0.0;
    int topNote = 84;       // Highest visible pitch (C6)

    enum class DragMode { none, move, resize };

    struct DragState
    {
        DragMode mode = DragMode::none;
        juce::ValueTree note;
        double mouseDownBeat = 0.0, originalStart = 0.0, originalLength = 0.0;
        int mouseDownPitch = 0, originalPitch = 0;
    };

    DragState drag;
    juce::ValueTree selectedNote;     // Notes are tracked by state, since MidiNote objects get recreated on undo

    //==============================================================================
    te::MidiClip* getClip() const;
    te::MidiNote* findNote (const juce::ValueTree&) const;
    te::MidiNote* findNoteAt (juce::Point<float>, bool& onRightEdge) const;

    double xToBeat (double x) const;
    double beatToX (double beat) const;
    int yToPitch (double y) const;
    double pitchToY (int pitch) const;
    double snapBeat (double beat) const;
    juce::Rectangle<float> getNoteBounds (const te::MidiNote&) const;
    static bool isBlackKey (int pitch);

    void deleteNote (te::MidiNote&);
    void paintKeyboard (juce::Graphics&);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
