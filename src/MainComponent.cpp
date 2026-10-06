#include "MainComponent.h"

namespace
{
    const juce::String projectExtension = ".opendaw";

    juce::String formatPosition (te::Edit& edit, te::TimePosition position)
    {
        const auto barsBeats = edit.tempoSequence.toBarsAndBeats (position);
        const auto millis = juce::roundToInt (position.inSeconds() * 1000.0);

        return juce::String::formatted ("%d.%d   %02d:%02d.%03d",
                                        barsBeats.bars + 1, barsBeats.getWholeBeats() + 1,
                                        millis / 60000, (millis / 1000) % 60, millis % 1000);
    }

    bool isCommand (const juce::KeyPress& key, juce::juce_wchar letter, bool withShift = false)
    {
        const auto mods = key.getModifiers();
        return mods.isCommandDown() && mods.isShiftDown() == withShift
                && juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode()) == letter;
    }
}

MainComponent::MainComponent()
{
    playButton.onClick = [this] { togglePlay(); };
    stopButton.onClick = [this] { stop(); };
    addTrackButton.onClick = [this] { arrangement->addTrack(); };
    addInstrumentButton.onClick = [this] { arrangement->addInstrumentTrack(); };
    settingsButton.onClick = [this] { showAudioSettings(); };
    closeEditorButton.onClick = [this] { closePianoRoll(); };
    snapButton.onClick = [this] { arrangement->setSnapToGrid (snapButton.getToggleState()); };
    snapButton.setToggleState (true, juce::dontSendNotification);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.onDragStart = [this] { edit->getUndoManager().beginNewTransaction(); };
    tempoSlider.onValueChange = [this] { edit->tempoSequence.getTempo (0)->setBpm (tempoSlider.getValue()); };

    positionLabel.setFont (juce::FontOptions (18.0f));
    positionLabel.setJustificationType (juce::Justification::centred);

    // Piano roll grid sizes, as fractions of a whole note (item ID = denominator)
    for (auto denominator : { 4, 8, 16, 32 })
        gridBox.addItem ("Grid 1/" + juce::String (denominator), denominator);

    gridBox.setSelectedId (16, juce::dontSendNotification);
    gridBox.onChange = [this]
    {
        if (pianoRoll != nullptr)
            pianoRoll->setGridSize (te::BeatDuration::fromBeats (4.0 / gridBox.getSelectedId()));
    };

    editorTitle.setFont (juce::FontOptions (14.0f, juce::Font::bold));

    for (auto* c : std::initializer_list<juce::Component*> { &menuBar, &playButton, &stopButton, &addTrackButton,
                                                              &addInstrumentButton, &settingsButton, &snapButton,
                                                              &tempoSlider, &tempoLabel, &positionLabel,
                                                              &editorTitle, &gridBox, &closeEditorButton })
    {
        // Keep keyboard focus on the arrangement so shortcuts keep working after clicking buttons
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }

    // The editor header is only shown while the piano roll is open
    for (auto* c : std::initializer_list<juce::Component*> { &editorTitle, &gridBox, &closeEditorButton })
        c->setVisible (false);

    setWantsKeyboardFocus (true);
    setEdit (te::createEmptyEdit (engine, juce::File()));
    setSize (1200, 700);
    startTimerHz (10);
}

MainComponent::~MainComponent()
{
    menuBar.setModel (nullptr);
    pianoRoll = nullptr;
    arrangement = nullptr;
    edit->getTransport().removeChangeListener (this);
}

//==============================================================================
void MainComponent::setEdit (std::unique_ptr<te::Edit> newEdit)
{
    if (newEdit == nullptr)
        return;

    closePianoRoll();
    arrangement = nullptr;

    if (edit != nullptr)
    {
        edit->getTransport().stop (false, false);
        edit->getTransport().removeChangeListener (this);
    }

    edit = std::move (newEdit);
    edit->getTransport().addChangeListener (this);
    edit->getUndoManager().clearUndoHistory();
    edit->resetChangedStatus();

    arrangement = std::make_unique<ArrangementView> (*edit);
    arrangement->setSnapToGrid (snapButton.getToggleState());
    arrangement->onOpenMidiClip = [this] (te::EditItemID clipID) { openPianoRoll (clipID); };
    addAndMakeVisible (*arrangement);

    tempoSlider.setValue (edit->tempoSequence.getTempo (0)->getBpm(), juce::dontSendNotification);
    playButton.setButtonText ("Play");

    resized();
    updateWindowTitle();
    arrangement->grabKeyboardFocus();
}

void MainComponent::openPianoRoll (te::EditItemID clipID)
{
    auto* clip = te::findClipForID (*edit, clipID);

    if (clip == nullptr)
        return;

    pianoRoll = std::make_unique<PianoRoll> (*edit, clipID);
    pianoRoll->setGridSize (te::BeatDuration::fromBeats (4.0 / gridBox.getSelectedId()));
    addAndMakeVisible (*pianoRoll);

    editorTitle.setText ("Piano Roll: " + clip->getName(), juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &editorTitle, &gridBox, &closeEditorButton })
        c->setVisible (true);

    resized();
    pianoRoll->grabKeyboardFocus();
}

void MainComponent::closePianoRoll()
{
    pianoRoll = nullptr;

    for (auto* c : std::initializer_list<juce::Component*> { &editorTitle, &gridBox, &closeEditorButton })
        c->setVisible (false);

    resized();

    if (arrangement != nullptr)
        arrangement->grabKeyboardFocus();
}

void MainComponent::newProject()
{
    confirmDiscardChanges ([this]
    {
        projectFile = juce::File();
        setEdit (te::createEmptyEdit (engine, juce::File()));
    });
}

void MainComponent::openProject()
{
    confirmDiscardChanges ([this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Open Project", getDefaultProjectFolder(),
                                                           "*" + projectExtension + ";*.tracktionedit");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file.existsAsFile())
                                          loadProject (file);
                                  });
    });
}

void MainComponent::loadProject (const juce::File& file)
{
    auto loaded = te::loadEditFromFile (engine, file);

    if (loaded == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open Project",
                                                "Couldn't open " + file.getFullPathName());
        return;
    }

    projectFile = file;
    setEdit (std::move (loaded));
}

void MainComponent::saveProject (bool forceChooseFile, std::function<void (bool)> onDone)
{
    if (! forceChooseFile && projectFile != juce::File())
    {
        const auto ok = writeProject (projectFile);

        if (onDone != nullptr)
            onDone (ok);

        return;
    }

    const auto initialFile = projectFile != juce::File()
                                ? projectFile
                                : getDefaultProjectFolder().getChildFile ("Untitled" + projectExtension);

    fileChooser = std::make_unique<juce::FileChooser> ("Save Project", initialFile, "*" + projectExtension);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, onDone] (const juce::FileChooser& chooser)
                              {
                                  auto file = chooser.getResult();
                                  const auto ok = file != juce::File()
                                                   && writeProject (file.withFileExtension (projectExtension));

                                  if (onDone != nullptr)
                                      onDone (ok);
                              });
}

bool MainComponent::writeProject (const juce::File& file)
{
    file.getParentDirectory().createDirectory();

    if (! te::EditFileOperations (*edit).writeToFile (file, false))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save Project",
                                                "Couldn't save to " + file.getFullPathName());
        return false;
    }

    projectFile = file;
    edit->resetChangedStatus();
    updateWindowTitle();
    return true;
}

void MainComponent::confirmDiscardChanges (std::function<void()> onProceed)
{
    if (! edit->hasChangedSinceSaved())
    {
        onProceed();
        return;
    }

    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                           "Unsaved Changes",
                                           "Do you want to save your changes to "
                                               + (projectFile == juce::File() ? juce::String ("this project")
                                                                              : projectFile.getFileName())
                                               + "?",
                                           "Save", "Don't Save", "Cancel", this,
                                           juce::ModalCallbackFunction::create ([this, onProceed] (int result)
                                           {
                                               if (result == 1)
                                                   saveProject (false, [onProceed] (bool saved) { if (saved) onProceed(); });
                                               else if (result == 2)
                                                   onProceed();
                                           }));
}

juce::File MainComponent::getDefaultProjectFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("OpenDAW");
}

//==============================================================================
void MainComponent::undo()
{
    edit->undo();
    tempoSlider.setValue (edit->tempoSequence.getTempo (0)->getBpm(), juce::dontSendNotification);
    arrangement->repaint();
}

void MainComponent::redo()
{
    edit->redo();
    tempoSlider.setValue (edit->tempoSequence.getTempo (0)->getBpm(), juce::dontSendNotification);
    arrangement->repaint();
}

void MainComponent::togglePlay()
{
    auto& transport = edit->getTransport();

    if (transport.isPlaying())
        transport.stop (false, false);
    else
        transport.play (false);
}

void MainComponent::stop()
{
    auto& transport = edit->getTransport();
    transport.stop (false, false);
    transport.setPosition (te::TimePosition());
}

void MainComponent::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Audio Settings";
    o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    o.content.setOwned (new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager,
                                                                0, 512, 1, 512, false, false, true, true));
    o.content->setSize (420, 520);
    o.launchAsync();
}

void MainComponent::updateWindowTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        const auto name = projectFile == juce::File() ? juce::String ("Untitled")
                                                      : projectFile.getFileNameWithoutExtension();
        const auto title = name + (edit->hasChangedSinceSaved() ? "*" : "") + " - OpenDAW";

        if (window->getName() != title)
            window->setName (title);
    }
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f22));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    menuBar.setBounds (area.removeFromTop (menuBarHeight));

    auto toolbar = area.removeFromTop (toolbarHeight).reduced (6);
    playButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (6);
    stopButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (16);
    tempoLabel.setBounds (toolbar.removeFromLeft (40));
    tempoSlider.setBounds (toolbar.removeFromLeft (140));
    toolbar.removeFromLeft (16);
    snapButton.setBounds (toolbar.removeFromLeft (70));
    addTrackButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (6);
    addInstrumentButton.setBounds (toolbar.removeFromLeft (100));
    settingsButton.setBounds (toolbar.removeFromRight (120));
    positionLabel.setBounds (toolbar);

    if (pianoRoll != nullptr)
    {
        auto editorArea = area.removeFromBottom (juce::jmax (220, area.getHeight() * 45 / 100));
        auto header = editorArea.removeFromTop (editorHeaderHeight).reduced (6, 3);

        closeEditorButton.setBounds (header.removeFromRight (70));
        header.removeFromRight (6);
        gridBox.setBounds (header.removeFromRight (110));
        editorTitle.setBounds (header);
        pianoRoll->setBounds (editorArea);
    }

    if (arrangement != nullptr)
        arrangement->setBounds (area);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)       { togglePlay(); return true; }
    if (key == juce::KeyPress::returnKey)      { stop(); return true; }
    if (isCommand (key, 'Z'))                  { undo(); return true; }
    if (isCommand (key, 'Y')
         || isCommand (key, 'Z', true))        { redo(); return true; }
    if (isCommand (key, 'N'))                  { newProject(); return true; }
    if (isCommand (key, 'O'))                  { openProject(); return true; }
    if (isCommand (key, 'S'))                  { saveProject (false); return true; }
    if (isCommand (key, 'S', true))            { saveProject (true); return true; }
    if (isCommand (key, 'T'))                  { arrangement->addTrack(); return true; }
    if (isCommand (key, 'I'))                  { arrangement->addInstrumentTrack(); return true; }
    if (key == juce::KeyPress::escapeKey
         && pianoRoll != nullptr)              { closePianoRoll(); return true; }

    return false;
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Plugins" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    auto addItem = [&menu] (const juce::String& name, const juce::String& shortcut, bool enabled, std::function<void()> action)
    {
        juce::PopupMenu::Item item (name);
        item.shortcutKeyDescription = shortcut;
        item.isEnabled = enabled;
        item.action = std::move (action);
        menu.addItem (std::move (item));
    };

    if (menuIndex == 0)
    {
        addItem ("New Project",      "Ctrl+N",       true, [this] { newProject(); });
        addItem ("Open Project...",  "Ctrl+O",       true, [this] { openProject(); });
        menu.addSeparator();
        addItem ("Save",             "Ctrl+S",       true, [this] { saveProject (false); });
        addItem ("Save As...",       "Ctrl+Shift+S", true, [this] { saveProject (true); });
        menu.addSeparator();
        addItem ("Audio Settings...", {},            true, [this] { showAudioSettings(); });
        menu.addSeparator();
        addItem ("Exit",             "Alt+F4",       true, [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
    }
    else if (menuIndex == 1)
    {
        auto& undoManager = edit->getUndoManager();
        addItem ("Undo",          "Ctrl+Z", undoManager.canUndo(), [this] { undo(); });
        addItem ("Redo",          "Ctrl+Y", undoManager.canRedo(), [this] { redo(); });
        menu.addSeparator();
        addItem ("Delete Clip",   "Del",    arrangement->hasSelectedClip(), [this] { arrangement->deleteSelectedClip(); });
        addItem ("Add Track",     "Ctrl+T", true, [this] { arrangement->addTrack(); });
        addItem ("Add Instrument Track", "Ctrl+I", true, [this] { arrangement->addInstrumentTrack(); });
        menu.addSeparator();
        menu.addItem ("Snap to Grid", true, snapButton.getToggleState(), [this]
        {
            snapButton.setToggleState (! snapButton.getToggleState(), juce::dontSendNotification);
            arrangement->setSnapToGrid (snapButton.getToggleState());
        });
    }
    else if (menuIndex == 2)
    {
        const auto numPlugins = engine.getPluginManager().knownPluginList.getNumTypes();
        addItem ("Scan for Plugins...", {}, true, [this] { showPluginScanner(); });
        menu.addItem (juce::String (numPlugins) + " plugins found. Right-click a track name to add them",
                      false, false, nullptr);
    }

    return menu;
}

void MainComponent::showPluginScanner()
{
    auto& pluginManager = engine.getPluginManager();

    auto* scanner = new juce::PluginListComponent (pluginManager.pluginFormatManager,
                                                   pluginManager.knownPluginList,
                                                   engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                                   std::addressof (engine.getPropertyStorage().getPropertiesFile()),
                                                   true);
    scanner->setSize (800, 600);

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Plugins";
    o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.content.setOwned (scanner);
    o.launchAsync();
}

//==============================================================================
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    playButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
}

void MainComponent::timerCallback()
{
    positionLabel.setText (formatPosition (*edit, edit->getTransport().getPosition()), juce::dontSendNotification);
    updateWindowTitle();
}
