#include "PluginWindow.h"

namespace
{
    /** Hosts an external plugin's own editor (or JUCE's generic editor if it has none). */
    struct ExternalPluginEditor : public te::Plugin::EditorComponent
    {
        explicit ExternalPluginEditor (te::ExternalPlugin& p)
            : plugin (p)
        {
            if (auto* instance = plugin.getAudioPluginInstance())
            {
                editor.reset (instance->createEditorAndMakeActive());

                if (editor == nullptr)
                    editor = std::make_unique<juce::GenericAudioProcessorEditor> (*instance);

                addAndMakeVisible (*editor);
            }

            resizeToFitEditor();
        }

        bool allowWindowResizing() override     { return false; }

        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override
        {
            return editor != nullptr ? editor->getConstrainer() : nullptr;
        }

        void resized() override
        {
            if (editor != nullptr)
                editor->setBounds (getLocalBounds());
        }

        void childBoundsChanged (juce::Component* c) override
        {
            if (c == editor.get())
            {
                plugin.edit.pluginChanged (plugin);
                resizeToFitEditor();
            }
        }

        void resizeToFitEditor()
        {
            setSize (juce::jmax (8, editor != nullptr ? editor->getWidth() : 0),
                     juce::jmax (8, editor != nullptr ? editor->getHeight() : 0));
        }

        te::ExternalPlugin& plugin;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
    };
}

//==============================================================================
PluginWindow::PluginWindow (te::Plugin& p)
    : DocumentWindow (p.getName(), juce::Colours::black, juce::DocumentWindow::closeButton, true),
      plugin (p)
{
    getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
    setResizeLimits (100, 50, 4000, 4000);
    recreateEditor();
    setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());
    updateStoredBounds = true;
}

PluginWindow::~PluginWindow()
{
    updateStoredBounds = false;
    plugin.edit.flushPluginStateIfNeeded (plugin);
    setEditor (nullptr);
}

std::unique_ptr<juce::Component> PluginWindow::create (te::Plugin& plugin)
{
    if (auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin))
        if (external->getAudioPluginInstance() == nullptr)
            return {};

    auto window = std::make_unique<PluginWindow> (plugin);

    if (window->getEditor() == nullptr)
        return {};

    window->setVisible (true);
    window->toFront (false);
    return window;
}

void PluginWindow::setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
{
    setConstrainer (nullptr);
    editor.reset();

    if (newEditor != nullptr)
    {
        editor = std::move (newEditor);
        setContentNonOwned (editor.get(), true);
    }

    setResizable (editor == nullptr || editor->allowWindowResizing(), false);

    if (editor != nullptr && editor->allowWindowResizing())
        setConstrainer (editor->getBoundsConstrainer());
}

void PluginWindow::recreateEditor()
{
    setEditor (nullptr);

    if (auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin))
        setEditor (std::make_unique<ExternalPluginEditor> (*external));
    else
        setEditor (plugin.createEditor());
}

void PluginWindow::recreateEditorAsync()
{
    setEditor (nullptr);

    juce::Timer::callAfterDelay (50, [safeThis = juce::Component::SafePointer<PluginWindow> (this)]
    {
        if (safeThis != nullptr)
            safeThis->recreateEditor();
    });
}

void PluginWindow::moved()
{
    if (updateStoredBounds)
    {
        plugin.windowState->lastWindowBounds = getBounds();
        plugin.edit.pluginChanged (plugin);
    }
}

//==============================================================================
std::unique_ptr<juce::Component> OpenDAWUIBehaviour::createPluginWindow (te::PluginWindowState& state)
{
    if (auto* windowState = dynamic_cast<te::Plugin::WindowState*> (&state))
        return PluginWindow::create (windowState->plugin);

    return {};
}

void OpenDAWUIBehaviour::recreatePluginWindowContentAsync (te::Plugin& plugin)
{
    if (auto* window = dynamic_cast<PluginWindow*> (plugin.windowState->pluginWindow.get()))
        return window->recreateEditorAsync();

    UIBehaviour::recreatePluginWindowContentAsync (plugin);
}
