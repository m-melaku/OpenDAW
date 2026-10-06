#include "PluginMenus.h"

namespace PluginMenus
{
namespace
{
    bool isFixedPlugin (te::Plugin* p)
    {
        return dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr
            || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr;
    }

    /** Wraps an action so it runs on a freshly looked-up chain, inside a new undo transaction. */
    std::function<void()> onChain (te::Edit& edit, te::EditItemID trackID, std::function<void (te::PluginList&, te::AudioTrack*)> action)
    {
        return [&edit, trackID, action]
        {
            if (auto* chain = findChain (edit, trackID))
            {
                edit.getUndoManager().beginNewTransaction();
                action (*chain, dynamic_cast<te::AudioTrack*> (te::findTrackForID (edit, trackID)));
            }
        };
    }

    juce::String describe (const juce::PluginDescription& desc)
    {
        return desc.name + " (" + desc.manufacturerName + ")";
    }
}

te::PluginList* findChain (te::Edit& edit, te::EditItemID trackID)
{
    if (trackID.isInvalid())
        return &edit.getMasterPluginList();

    if (auto* track = dynamic_cast<te::AudioTrack*> (te::findTrackForID (edit, trackID)))
        return &track->pluginList;

    return nullptr;
}

juce::PopupMenu createInstrumentMenu (te::Edit& edit, te::EditItemID trackID)
{
    juce::PopupMenu menu;
    auto& cache = edit.getPluginCache();

    menu.addItem ("4OSC (built-in synth)", onChain (edit, trackID, [&cache] (te::PluginList&, te::AudioTrack* track)
    {
        if (track != nullptr)
            setInstrument (*track, cache.createNewPlugin (te::FourOscPlugin::xmlTypeName, {}));
    }));

    menu.addSeparator();

    for (const auto& desc : edit.engine.getPluginManager().knownPluginList.getTypes())
        if (desc.isInstrument)
            menu.addItem (describe (desc), onChain (edit, trackID, [&cache, desc] (te::PluginList&, te::AudioTrack* track)
            {
                if (track != nullptr)
                    setInstrument (*track, cache.createNewPlugin (te::ExternalPlugin::xmlTypeName, desc));
            }));

    return menu;
}

juce::PopupMenu createEffectMenu (te::Edit& edit, te::EditItemID trackID)
{
    juce::PopupMenu menu;
    auto& cache = edit.getPluginCache();

    const std::pair<const char*, const char*> builtInEffects[] = {
        { "EQ",         te::EqualiserPlugin::xmlTypeName },
        { "Compressor", te::CompressorPlugin::xmlTypeName },
        { "Reverb",     te::ReverbPlugin::xmlTypeName },
        { "Delay",      te::DelayPlugin::xmlTypeName },
        { "Chorus",     te::ChorusPlugin::xmlTypeName },
        { "Phaser",     te::PhaserPlugin::xmlTypeName },
        { "Low Pass",   te::LowPassPlugin::xmlTypeName },
    };

    for (const auto& [name, type] : builtInEffects)
        menu.addItem (juce::String (name) + " (built-in)", onChain (edit, trackID, [&cache, type] (te::PluginList& chain, te::AudioTrack*)
        {
            addEffect (chain, cache.createNewPlugin (type, {}));
        }));

    menu.addSeparator();

    for (const auto& desc : edit.engine.getPluginManager().knownPluginList.getTypes())
        if (! desc.isInstrument)
            menu.addItem (describe (desc), onChain (edit, trackID, [&cache, desc] (te::PluginList& chain, te::AudioTrack*)
            {
                addEffect (chain, cache.createNewPlugin (te::ExternalPlugin::xmlTypeName, desc));
            }));

    return menu;
}

juce::PopupMenu createPluginMenu (te::Edit& edit, te::Plugin& plugin)
{
    juce::PopupMenu menu;
    te::Plugin::Ptr ref (&plugin);

    menu.addItem ("Open Editor", [ref] { ref->windowState->showWindowExplicitly(); });
    menu.addItem ("Bypass", true, ! plugin.isEnabled(), [&edit, ref]
    {
        edit.getUndoManager().beginNewTransaction();
        ref->setEnabled (! ref->isEnabled());
    });
    menu.addItem ("Remove", [&edit, ref]
    {
        edit.getUndoManager().beginNewTransaction();
        ref->deleteFromParent();
    });

    return menu;
}

juce::Array<te::Plugin*> getUserPlugins (te::PluginList& chain)
{
    juce::Array<te::Plugin*> result;

    for (auto* p : chain.getPlugins())
        if (! isFixedPlugin (p))
            result.add (p);

    return result;
}

void setInstrument (te::AudioTrack& track, te::Plugin::Ptr instrument)
{
    if (instrument == nullptr)
        return;

    for (auto* plugin : track.pluginList.getPlugins())
        if (plugin->isSynth())
            plugin->deleteFromParent();

    track.pluginList.insertPlugin (instrument, 0, nullptr);

    if (dynamic_cast<te::ExternalPlugin*> (instrument.get()) != nullptr)
        instrument->windowState->showWindowExplicitly();
}

void addEffect (te::PluginList& chain, te::Plugin::Ptr effect)
{
    if (effect == nullptr)
        return;

    // Effects go before the first fader/meter, so those stay at the end of the chain
    int index = -1;
    const auto plugins = chain.getPlugins();

    for (int i = 0; i < plugins.size(); ++i)
    {
        if (isFixedPlugin (plugins[i]))
        {
            index = i;
            break;
        }
    }

    chain.insertPlugin (effect, index, nullptr);

    if (dynamic_cast<te::ExternalPlugin*> (effect.get()) != nullptr)
        effect->windowState->showWindowExplicitly();
}

void ensureInstrument (te::AudioTrack& track)
{
    for (auto* plugin : track.pluginList.getPlugins())
        if (plugin->isSynth())
            return;

    // Every MIDI track needs something to make sound, so default to the built-in 4OSC synth
    if (auto synth = track.edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
        track.pluginList.insertPlugin (synth, 0, nullptr);
}

te::LevelMeterPlugin* ensureMasterMeter (te::Edit& edit)
{
    auto& chain = edit.getMasterPluginList();

    for (auto* p : chain.getPlugins())
        if (auto* meter = dynamic_cast<te::LevelMeterPlugin*> (p))
            return meter;

    if (auto meter = edit.getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, {}))
    {
        chain.insertPlugin (meter, -1, nullptr);
        return dynamic_cast<te::LevelMeterPlugin*> (meter.get());
    }

    return nullptr;
}
}
