#pragma once

#include <JuceHeader.h>
#include <map>

namespace te = tracktion;

/** The timeline: a bar/beat ruler, track headers and clips.

    - Drag a clip to move it (in time and between tracks), drag its edges to trim it
    - Drop audio files onto a track to insert them at that time
    - Ctrl+wheel zooms, Shift+wheel scrolls horizontally, wheel scrolls tracks
    - Click or drag in the ruler to move the playhead
*/
class ArrangementView : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit ArrangementView (te::Edit&);
    ~ArrangementView() override;

    void addTrack();
    void addInstrumentTrack();
    void deleteSelectedClip();
    bool hasSelectedClip() const;

    /** Called when a MIDI clip is double-clicked, or a new one is created. */
    std::function<void (te::EditItemID)> onOpenMidiClip;

    void setSnapToGrid (bool shouldSnap)    { snapToGrid = shouldSnap; }
    bool isSnapToGrid() const               { return snapToGrid; }

    //==============================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

private:
    te::Edit& edit;

    static constexpr int rulerHeight = 24, headerWidth = 160, trackHeight = 70, edgeGrabWidth = 6;
    static constexpr double minClipLength = 0.01;

    double pixelsPerSecond = 40.0;
    double scrollSeconds = 0.0;
    int scrollY = 0;
    bool snapToGrid = true;
    bool isDraggingFiles = false;

    enum class DragMode { none, move, trimStart, trimEnd, scrub };

    struct DragState
    {
        DragMode mode = DragMode::none;
        te::EditItemID clipID;
        double mouseDownTime = 0.0, originalStart = 0.0, originalEnd = 0.0, originalOffset = 0.0, maxEnd = 0.0;
    };

    DragState drag;
    te::EditItemID selectedClipID;
    std::map<juce::String, std::unique_ptr<te::SmartThumbnail>> thumbnails;

    //==============================================================================
    juce::Array<te::AudioTrack*> getTracks() const     { return te::getAudioTracks (edit); }

    double timeToX (double seconds) const;
    double xToTime (double x) const;
    double snap (double seconds) const;
    int trackIndexAtY (int y) const;
    int getTrackY (int index) const;
    int getMaxScrollY() const;

    juce::Rectangle<float> getClipBounds (te::Clip&, int trackIndex) const;
    te::Clip* findClipAt (juce::Point<float>, DragMode& modeOut) const;
    juce::Rectangle<int> getMuteButtonBounds (int trackIndex) const;
    juce::Rectangle<int> getSoloButtonBounds (int trackIndex) const;

    te::SmartThumbnail& getThumbnail (te::AudioClipBase&);
    te::AudioTrack& getTrackForDrop (int trackIndex);
    void ensureInstrument (te::AudioTrack&);
    void showTrackMenu (te::AudioTrack&);

    void paintRuler (juce::Graphics&);
    void paintTrack (juce::Graphics&, te::AudioTrack&, int trackIndex);
    void paintClip (juce::Graphics&, te::Clip&, juce::Rectangle<float>);
    void paintHeader (juce::Graphics&, te::AudioTrack&, int trackIndex);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementView)
};
