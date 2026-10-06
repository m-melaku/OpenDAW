#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/** Menus and helpers for putting plugins on tracks, shared by the arrangement and the mixer.

    A "chain" is either a track's plugin list or the master plugin list. Every menu action
    looks its chain up again when it runs, and wraps its change in a new undo transaction.
*/
namespace PluginMenus
{
    /** Returns the plugin list for a track, or the master plugin list if trackID is invalid. */
    te::PluginList* findChain (te::Edit&, te::EditItemID trackID);

    /** Instruments: the built-in 4OSC plus any scanned VST3 instruments. Replaces the current instrument. */
    juce::PopupMenu createInstrumentMenu (te::Edit&, te::EditItemID trackID);

    /** Effects: built-ins plus any scanned VST3 effects. Inserted before the track's fader and meter. */
    juce::PopupMenu createEffectMenu (te::Edit&, te::EditItemID trackID);

    /** Open Editor / Bypass / Remove for one plugin. */
    juce::PopupMenu createPluginMenu (te::Edit&, te::Plugin&);

    /** Plugins the user put on the chain (excluding the built-in fader and meter). */
    juce::Array<te::Plugin*> getUserPlugins (te::PluginList&);

    void setInstrument (te::AudioTrack&, te::Plugin::Ptr);
    void addEffect (te::PluginList&, te::Plugin::Ptr);
    void ensureInstrument (te::AudioTrack&);

    /** Makes sure the master chain ends with a level meter, for the mixer's master meter. */
    te::LevelMeterPlugin* ensureMasterMeter (te::Edit&);
}
