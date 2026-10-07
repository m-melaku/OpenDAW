#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/** A floating window that shows a plugin's editor.
    Adapted from Tracktion Engine's examples/common/PluginWindow.h (GPLv3).
*/
class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin&);
    ~PluginWindow() override;

    static std::unique_ptr<juce::Component> create (te::Plugin&);

    void recreateEditor();
    void recreateEditorAsync();
    te::Plugin::EditorComponent* getEditor() const     { return editor.get(); }

private:
    te::Plugin& plugin;
    std::unique_ptr<te::Plugin::EditorComponent> editor;
    bool updateStoredBounds = false;

    void setEditor (std::unique_ptr<te::Plugin::EditorComponent>);
    void moved() override;
    void closeButtonPressed() override              { plugin.windowState->closeWindowExplicitly(); }
    float getDesktopScaleFactor() const override    { return 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

//==============================================================================
/** Tells the engine how to create plugin windows. Pass to the te::Engine constructor. */
class OpenDAWUIBehaviour : public te::UIBehaviour
{
public:
    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState&) override;
    void recreatePluginWindowContentAsync (te::Plugin&) override;
};
