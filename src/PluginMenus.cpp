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

    /** Sends and returns are shown as bus controls rather than plugin slots. */
    bool isRoutingPlugin (te::Plugin* p)
    {
        return dynamic_cast<te::AuxSendPlugin*> (p) != nullptr
            || dynamic_cast<te::AuxReturnPlugin*> (p) != nullptr;
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
        if (! isFixedPlugin (p) && ! isRoutingPlugin (p))
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

//==============================================================================
int getBusNumber (te::AudioTrack& track)
{
    for (auto* p : track.pluginList.getPlugins())
        if (auto* ret = dynamic_cast<te::AuxReturnPlugin*> (p))
            return ret->busNumber.get();

    return -1;
}

juce::String getBusTrackName (te::Edit& edit, int busNumber)
{
    for (auto* track : te::getAudioTracks (edit))
        if (getBusNumber (*track) == busNumber)
            return track->getName();

    return "Bus " + juce::String (busNumber + 1);
}

te::AudioTrack* addBusTrack (te::Edit& edit)
{
    int busNumber = 0;

    for (auto* track : te::getAudioTracks (edit))
        busNumber = juce::jmax (busNumber, getBusNumber (*track) + 1);

    edit.ensureNumberOfAudioTracks (te::getAudioTracks (edit).size() + 1);
    auto* track = te::getAudioTracks (edit).getLast();
    track->setName ("Bus " + juce::String (busNumber + 1));

    if (auto plugin = edit.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
    {
        if (auto* ret = dynamic_cast<te::AuxReturnPlugin*> (plugin.get()))
            ret->busNumber = busNumber;

        track->pluginList.insertPlugin (plugin, 0, nullptr);
    }

    return track;
}

juce::Array<te::AuxSendPlugin*> getSends (te::PluginList& chain)
{
    juce::Array<te::AuxSendPlugin*> sends;

    for (auto* p : chain.getPlugins())
        if (auto* send = dynamic_cast<te::AuxSendPlugin*> (p))
            sends.add (send);

    return sends;
}

juce::PopupMenu createSendMenu (te::Edit& edit, te::EditItemID trackID)
{
    juce::PopupMenu menu;
    auto* source = dynamic_cast<te::AudioTrack*> (te::findTrackForID (edit, trackID));

    if (source == nullptr)
        return menu;

    juce::Array<int> existingSends;

    for (auto* send : getSends (source->pluginList))
        existingSends.add (send->busNumber.get());

    for (auto* bus : te::getAudioTracks (edit))
    {
        const auto busNumber = getBusNumber (*bus);

        if (busNumber < 0 || bus == source)
            continue;

        menu.addItem ("Send to " + bus->getName(), ! existingSends.contains (busNumber), false,
                      onChain (edit, trackID, [&edit, busNumber] (te::PluginList& chain, te::AudioTrack* track)
        {
            auto plugin = edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});

            if (plugin == nullptr || track == nullptr)
                return;

            if (auto* send = dynamic_cast<te::AuxSendPlugin*> (plugin.get()))
            {
                send->busNumber = busNumber;
                send->setGainDb (0.0f);
            }

            // Post-fader: right after the track's volume plugin
            chain.insertPlugin (plugin, chain.indexOf (track->getVolumePlugin()) + 1, nullptr);
        }));
    }

    if (menu.getNumItems() == 0)
        menu.addItem ("(No buses yet: use Edit > Add Bus Track)", false, false, nullptr);

    return menu;
}
}
