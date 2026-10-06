#pragma once

#include <JuceHeader.h>
#include "ArrangementView.h"
#include "PianoRoll.h"
#include "MixerView.h"
#include "PluginWindow.h"

namespace te = tracktion;

/** The main window content: menu bar, transport toolbar and the arrangement.
    Owns the Engine and the current Edit (project).
*/
class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    /** Asks to save unsaved changes, then calls onProceed unless the user cancels. */
    void confirmDiscardChanges (std::function<void()> onProceed);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int, int) override {}

private:
    // The Engine must outlive the Edit, and the Edit must outlive the ArrangementView
    te::Engine engine { ProjectInfo::projectName, std::make_unique<OpenDAWUIBehaviour>(), nullptr };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<ArrangementView> arrangement;
    std::unique_ptr<PianoRoll> pianoRoll;
    std::unique_ptr<MixerView> mixer;
    juce::File projectFile;     // Empty until the project is first saved
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::MenuBarComponent menuBar { this };
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, addTrackButton { "+ Track" },
                     addInstrumentButton { "+ Instrument" }, mixerButton { "Mixer" },
                     settingsButton { "Audio Settings" },
                     closeEditorButton { "Close" };
    juce::ToggleButton snapButton { "Snap" };
    juce::Slider tempoSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label tempoLabel { {}, "BPM" }, positionLabel, editorTitle;
    juce::ComboBox gridBox;

    static constexpr int menuBarHeight = 24, toolbarHeight = 40, editorHeaderHeight = 30;

    void setEdit (std::unique_ptr<te::Edit>);
    void openPianoRoll (te::EditItemID clipID);
    void toggleMixer();
    void closeBottomPanel();
    void showBottomPanelHeader (const juce::String& title, bool showGrid);
    void newProject();
    void openProject();
    void loadProject (const juce::File&);
    void saveProject (bool forceChooseFile, std::function<void (bool)> onDone = nullptr);
    bool writeProject (const juce::File&);

    void undo();
    void redo();
    void togglePlay();
    void stop();
    void showAudioSettings();
    void showPluginScanner();
    void routeMidiInputTo (te::EditItemID trackID);
    void updateWindowTitle();

    static juce::File getDefaultProjectFolder();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
