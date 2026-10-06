#include "MixerView.h"
#include "PluginMenus.h"

namespace
{
    namespace Palette
    {
        const juce::Colour background  { 0xff1e1f22 };
        const juce::Colour strip       { 0xff2e3035 };
        const juce::Colour master      { 0xff353842 };
        const juce::Colour meterBack   { 0xff151618 };
        const juce::Colour meterLow    { 0xff4fc36b };
        const juce::Colour meterMid    { 0xffe0c23a };
        const juce::Colour meterHigh   { 0xffe05050 };
        const juce::Colour mute        { 0xffe0b23a };
        const juce::Colour solo        { 0xff4fc3a1 };
    }

    constexpr float minDb = -60.0f, maxDb = 6.0f;
}

//==============================================================================
LevelMeter::~LevelMeter()
{
    setSource (nullptr);
}

te::LevelMeasurer* LevelMeter::getMeasurer() const
{
    if (auto* meterPlugin = dynamic_cast<te::LevelMeterPlugin*> (source.get()))
        return &meterPlugin->measurer;

    return nullptr;
}

void LevelMeter::setSource (te::LevelMeterPlugin* newSource)
{
    if (newSource == source.get())
        return;

    if (auto* measurer = getMeasurer())
        measurer->removeClient (client);

    source = newSource;

    if (auto* measurer = getMeasurer())
        measurer->addClient (client);
}

void LevelMeter::timerCallback()
{
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto newLevel = getMeasurer() != nullptr ? client.getAndClearAudioLevel (channel).dB : -100.0f;

        // Jump up instantly, fall back gradually
        levels[channel] = juce::jmax (newLevel, levels[channel] - 1.5f);
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::meterBack);
    g.fillRoundedRectangle (area, 2.0f);

    const auto barWidth = (area.getWidth() - 3.0f) / 2.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto proportion = juce::jlimit (0.0f, 1.0f, (levels[channel] - minDb) / (maxDb - minDb));
        auto bar = juce::Rectangle<float> (area.getX() + 1.0f + channel * (barWidth + 1.0f), area.getY() + 1.0f,
                                           barWidth, area.getHeight() - 2.0f);
        const auto filled = bar.removeFromBottom (bar.getHeight() * proportion);

        const auto colour = levels[channel] > 0.0f ? Palette::meterHigh
                          : levels[channel] > -6.0f ? Palette::meterMid
                          : Palette::meterLow;
        g.setColour (colour);
        g.fillRect (filled);
    }

    // 0 dB marker
    const auto zeroY = area.getY() + area.getHeight() * (1.0f - (0.0f - minDb) / (maxDb - minDb));
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawHorizontalLine ((int) zeroY, area.getX(), area.getRight());
}

//==============================================================================
ChannelStrip::ChannelStrip (te::Edit& e, te::EditItemID id)
    : edit (e), trackID (id)
{
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (nameLabel);

    addEffectButton.onClick = [this]
    {
        PluginMenus::createEffectMenu (edit, trackID)
            .showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addEffectButton));
    };
    addAndMakeVisible (addEffectButton);

    panKnob.setRange (-1.0, 1.0);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.setTooltip ("Pan (double-click to centre)");
    panKnob.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); };
    panKnob.onValueChange = [this]
    {
        if (auto* vol = getVolumePlugin())
            vol->setPan ((float) panKnob.getValue());
    };
    addAndMakeVisible (panKnob);

    fader.setRange (minDb, maxDb, 0.1);
    fader.setSkewFactorFromMidPoint (-12.0);
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setTextValueSuffix (" dB");
    fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    fader.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); };
    fader.onValueChange = [this]
    {
        if (auto* vol = getVolumePlugin())
            vol->setVolumeDb ((float) fader.getValue());
    };
    addAndMakeVisible (fader);

    if (isMaster())
    {
        nameLabel.setText ("Master", juce::dontSendNotification);
        meter.setSource (PluginMenus::ensureMasterMeter (edit));
    }
    else
    {
        muteButton.setClickingTogglesState (false);
        soloButton.setClickingTogglesState (false);
        muteButton.setColour (juce::TextButton::buttonOnColourId, Palette::mute);
        soloButton.setColour (juce::TextButton::buttonOnColourId, Palette::solo);

        muteButton.onClick = [this]
        {
            if (auto* t = getTrack())
            {
                edit.getUndoManager().beginNewTransaction();
                t->setMute (! t->isMuted (false));
            }
        };

        soloButton.onClick = [this]
        {
            if (auto* t = getTrack())
            {
                edit.getUndoManager().beginNewTransaction();
                t->setSolo (! t->isSolo (false));
            }
        };

        addAndMakeVisible (muteButton);
        addAndMakeVisible (soloButton);

        if (auto* t = getTrack())
            meter.setSource (t->getLevelMeterPlugin());
    }

    addAndMakeVisible (meter);

    for (auto* c : std::initializer_list<juce::Component*> { &addEffectButton, &panKnob, &fader, &muteButton, &soloButton })
        c->setWantsKeyboardFocus (false);

    timerCallback();
    startTimerHz (15);
}

te::AudioTrack* ChannelStrip::getTrack() const
{
    return dynamic_cast<te::AudioTrack*> (te::findTrackForID (edit, trackID));
}

te::VolumeAndPanPlugin* ChannelStrip::getVolumePlugin() const
{
    if (isMaster())
        return edit.getMasterVolumePlugin().get();

    if (auto* t = getTrack())
        return t->getVolumePlugin();

    return nullptr;
}

void ChannelStrip::rebuildPluginButtons()
{
    pluginButtons.clear();

    auto* chain = PluginMenus::findChain (edit, trackID);

    if (chain == nullptr)
        return;

    for (auto* plugin : PluginMenus::getUserPlugins (*chain))
    {
        auto* button = pluginButtons.add (new PluginSlot (plugin->getName()));
        te::Plugin::Ptr ref (plugin);

        button->setTooltip (plugin->getName() + " (click to open, right-click for options)");
        button->setWantsKeyboardFocus (false);
        button->setAlpha (plugin->isEnabled() ? 1.0f : 0.45f);
        button->onClickWithButton = [this, ref, button] (bool isRightClick)
        {
            if (isRightClick)
                PluginMenus::createPluginMenu (edit, *ref)
                    .showMenuAsync (juce::PopupMenu::Options().withTargetComponent (button));
            else
                ref->windowState->showWindowExplicitly();
        };

        addAndMakeVisible (button);
    }

    resized();
}

void ChannelStrip::timerCallback()
{
    // Pull state from the model, so undo/redo and changes made elsewhere show up here
    if (auto* t = getTrack())
    {
        nameLabel.setText (t->getName(), juce::dontSendNotification);
        muteButton.setToggleState (t->isMuted (false), juce::dontSendNotification);
        soloButton.setToggleState (t->isSolo (false), juce::dontSendNotification);
    }

    if (auto* vol = getVolumePlugin())
    {
        if (! fader.isMouseButtonDown())
            fader.setValue (juce::jlimit (minDb, maxDb, vol->getVolumeDb()), juce::dontSendNotification);

        if (! panKnob.isMouseButtonDown())
            panKnob.setValue (vol->getPan(), juce::dontSendNotification);
    }

    // Rebuild the plugin slots only when the chain (or a bypass state) changes
    juce::String signature;

    if (auto* chain = PluginMenus::findChain (edit, trackID))
        for (auto* p : PluginMenus::getUserPlugins (*chain))
            signature << p->itemID.toString() << (p->isEnabled() ? "+" : "-") << p->getName() << ";";

    if (signature != pluginSignature)
    {
        pluginSignature = signature;
        rebuildPluginButtons();
    }
}

void ChannelStrip::paint (juce::Graphics& g)
{
    g.setColour (isMaster() ? Palette::master : Palette::strip);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
}

void ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced (6);

    nameLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);

    for (auto* button : pluginButtons)
    {
        button->setBounds (area.removeFromTop (20));
        area.removeFromTop (2);
    }

    addEffectButton.setBounds (area.removeFromTop (20));
    area.removeFromTop (6);

    panKnob.setBounds (area.removeFromTop (36).withSizeKeepingCentre (36, 36));
    area.removeFromTop (4);

    if (! isMaster())
    {
        auto buttons = area.removeFromBottom (22);
        muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
        soloButton.setBounds (buttons.reduced (1, 0));
        area.removeFromBottom (4);
    }

    meter.setBounds (area.removeFromRight (14).withTrimmedBottom (22));
    area.removeFromRight (2);
    fader.setBounds (area);
}

//==============================================================================
MixerView::MixerView (te::Edit& e)
    : edit (e)
{
    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    masterStrip = std::make_unique<ChannelStrip> (edit, te::EditItemID());
    addAndMakeVisible (*masterStrip);

    rebuildIfTracksChanged();
    startTimerHz (4);
}

void MixerView::rebuildIfTracksChanged()
{
    const auto tracks = te::getAudioTracks (edit);
    bool changed = tracks.size() != strips.size();

    for (int i = 0; ! changed && i < tracks.size(); ++i)
        changed = tracks[i]->itemID != strips[i]->getTrackID();

    if (! changed)
        return;

    strips.clear();

    for (auto* track : tracks)
        stripHolder.addAndMakeVisible (strips.add (new ChannelStrip (edit, track->itemID)));

    resized();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
}

void MixerView::resized()
{
    auto area = getLocalBounds();
    masterStrip->setBounds (area.removeFromRight (ChannelStrip::width + 8).withTrimmedLeft (8));

    viewport.setBounds (area);

    const auto stripHeight = area.getHeight() - viewport.getScrollBarThickness();
    stripHolder.setSize (juce::jmax (area.getWidth(), strips.size() * ChannelStrip::width), stripHeight);

    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (i * ChannelStrip::width, 0, ChannelStrip::width, stripHeight);
}
