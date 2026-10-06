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

    //==============================================================================
    /** A slider per parameter, for built-in plugins that have no editor of their own. */
    struct GenericPluginEditor : public te::Plugin::EditorComponent,
                                 private juce::Timer
    {
        explicit GenericPluginEditor (te::Plugin& p)
        {
            for (auto* param : p.getAutomatableParameters())
            {
                auto row = std::make_unique<Row> (*param);
                content.addAndMakeVisible (*row);
                rows.push_back (std::move (row));
            }

            viewport.setViewedComponent (&content, false);
            viewport.setScrollBarsShown (true, false);
            addAndMakeVisible (viewport);

            const auto contentHeight = juce::jmax (rowHeight, (int) rows.size() * rowHeight);
            content.setSize (width - viewport.getScrollBarThickness(), contentHeight);
            setSize (width, juce::jmin (600, contentHeight + 8));

            startTimerHz (10);
        }

        bool allowWindowResizing() override                                 { return false; }
        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override   { return nullptr; }

        void resized() override
        {
            viewport.setBounds (getLocalBounds().reduced (0, 4));

            auto area = content.getLocalBounds().reduced (8, 0);

            for (auto& row : rows)
                row->setBounds (area.removeFromTop (rowHeight));
        }

    private:
        static constexpr int width = 460, rowHeight = 28;

        struct Row : public juce::Component
        {
            explicit Row (te::AutomatableParameter& p)
                : param (&p)
            {
                name.setText (p.getParameterName(), juce::dontSendNotification);
                addAndMakeVisible (name);

                slider.setSliderStyle (juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
                slider.setRange (0.0, 1.0);
                slider.setValue (p.getCurrentNormalisedValue(), juce::dontSendNotification);
                slider.textFromValueFunction = [this] (double v)
                {
                    return param->valueToString (param->valueRange.convertFrom0to1 ((float) v)) + param->getLabel();
                };
                slider.onDragStart = [this] { param->getEdit().getUndoManager().beginNewTransaction(); };
                slider.onValueChange = [this] { param->setNormalisedParameter ((float) slider.getValue(), juce::sendNotification); };
                slider.updateText();
                addAndMakeVisible (slider);
            }

            void resized() override
            {
                auto r = getLocalBounds();
                name.setBounds (r.removeFromLeft (150));
                slider.setBounds (r);
            }

            void refresh()
            {
                if (! slider.isMouseButtonDown())
                    slider.setValue (param->getCurrentNormalisedValue(), juce::dontSendNotification);
            }

            te::AutomatableParameter::Ptr param;
            juce::Label name;
            juce::Slider slider;
        };

        void timerCallback() override
        {
            // Keep sliders in sync with undo/redo and automation
            for (auto& row : rows)
                row->refresh();
        }

        juce::Viewport viewport;
        juce::Component content;
        std::vector<std::unique_ptr<Row>> rows;
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
    else if (auto builtInEditor = plugin.createEditor())
        setEditor (std::move (builtInEditor));
    else
        setEditor (std::make_unique<GenericPluginEditor> (plugin));
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
