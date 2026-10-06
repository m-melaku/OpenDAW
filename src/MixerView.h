#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/** A stereo peak meter that reads from a LevelMeterPlugin. */
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    LevelMeter() { startTimerHz (30); }
    ~LevelMeter() override;

    void setSource (te::LevelMeterPlugin*);
    void paint (juce::Graphics&) override;

private:
    te::Plugin::Ptr source;     // Keeps the plugin (and its measurer) alive while we're attached
    te::LevelMeasurer::Client client;
    float levels[2] { -100.0f, -100.0f };

    te::LevelMeasurer* getMeasurer() const;
    void timerCallback() override;
};

//==============================================================================
/** One mixer channel: plugin slots, pan, fader, meter, mute and solo.
    A strip for an invalid track ID is the master channel.
*/
class ChannelStrip : public juce::Component,
                     private juce::Timer
{
public:
    ChannelStrip (te::Edit&, te::EditItemID trackID);

    te::EditItemID getTrackID() const     { return trackID; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int width = 96;

private:
    te::Edit& edit;
    const te::EditItemID trackID;

    juce::Label nameLabel;
    /** A plugin slot: click opens the editor, right-click shows Bypass/Remove. */
    struct PluginSlot : public juce::TextButton
    {
        using juce::TextButton::TextButton;
        std::function<void (bool isRightClick)> onClickWithButton;
        void clicked (const juce::ModifierKeys& mods) override
        {
            if (onClickWithButton != nullptr)
                onClickWithButton (mods.isPopupMenu());
        }
    };

    juce::OwnedArray<PluginSlot> pluginButtons;
    juce::TextButton addEffectButton { "+ FX" };
    juce::Slider panKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    LevelMeter meter;
    juce::String pluginSignature;

    te::AudioTrack* getTrack() const;
    te::VolumeAndPanPlugin* getVolumePlugin() const;
    bool isMaster() const   { return trackID.isInvalid(); }

    void rebuildPluginButtons();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

//==============================================================================
/** All track channels side by side, with the master channel pinned on the right. */
class MixerView : public juce::Component,
                  private juce::Timer
{
public:
    explicit MixerView (te::Edit&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    te::Edit& edit;
    juce::Viewport viewport;
    juce::Component stripHolder;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> masterStrip;

    void rebuildIfTracksChanged();
    void timerCallback() override       { rebuildIfTracksChanged(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};
