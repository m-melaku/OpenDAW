# OpenDAW: notes for Claude

A Windows DAW built with C++20, JUCE 8.0.13 and Tracktion Engine 3.5 (`external/tracktion_engine`, shallow submodule; JUCE is nested inside it at `modules/juce`). The README's Roadmap section lists the milestones.

## Building
- **The user's machine can't build.** It has no compiler and almost no free disk space. Never try a local build.
- CI (`.github/workflows/build.yml`) builds with Ninja and MSVC through sccache; a warm build takes about 1 minute.
- Pushes to `main` or `milestone-*` update the `latest` release. The user tests from https://github.com/m-melaku/OpenDAW/releases/download/latest/OpenDAW.exe
- To find compile errors: `gh run view <id> --log-failed | grep -E "error C|error LNK"`. Ninja stops at the first failing file, so other files may not have been compiled yet.

## Workflow
- Name feature branches `milestone-N-...` so pushes publish to `latest`. Open a PR to `main`. **The user merges, never Claude.**
- Don't stack PRs on each other's branches. GitHub merged stacked PRs into their parent branches instead of `main`.
- Ask the user to merge with **Create a merge commit**, not squash. Squashing rewrites history and causes false conflicts with open branches. Fix those with `git merge -s ours origin/main`, but only after checking that the branch is a superset of `main`.
- Don't run CI monitors unless asked. Keep replies short.
- End commit messages with the attribution line given in the session.

## Source layout (`src/`)
| File | Contents |
|---|---|
| `Main.cpp` | App and window; quitting asks to save unsaved changes |
| `MainComponent` | Owns `te::Engine` and `te::Edit`; menus, toolbar, shortcuts, save/load (`.opendaw`), bottom panel (piano roll or mixer), MIDI input routing |
| `ArrangementView` | Ruler, track headers (A/M/S), clips (move, trim, split), file drop, automation lanes. Rows have variable height, so always use `getTrackY` and `trackIndexAtY`, never `index * trackHeight` |
| `PianoRoll` | Note editor for one `MidiClip`; notes are tracked by their `ValueTree` |
| `MixerView` | `LevelMeter`, `ChannelStrip` (plugin slots, sends, pan, fader, M/S), `MixerView` |
| `PluginMenus` | Shared instrument, effect, send and bus menus and helpers; `getUserPlugins` hides volume, meter and aux plugins |
| `PluginWindow` | Plugin editor windows plus `OpenDAWUIBehaviour`; falls back to a generic slider editor for built-in plugins |

## Conventions
- Look items up by `EditItemID` (`te::findClipForID`, `te::findTrackForID`) inside callbacks rather than holding raw pointers.
- Every user edit calls `edit.getUndoManager().beginNewTransaction()` first.
- UI components poll the model on a timer, so undo/redo shows up without listeners.
- Audio clips store absolute file paths (`getSourceFileReference().source.setValue(path, nullptr)`).
- Slider drags that change an `AutomatableParameter` must call `parameterChangeGestureBegin()` and `parameterChangeGestureEnd()` (see `attachGesture` in `MixerView.cpp`) so automation recording works.
- `JuceHeader.h` includes `using namespace juce`, so don't name things `Colours` (use `Palette`).
- Files switch to CRLF after `git checkout`, so multi-line `perl`/`sed` edits fail silently. Use the Edit tool.

## Verified Tracktion APIs (no need to grep again)
- Edit: `te::createEmptyEdit(engine, File())`, `te::loadEditFromFile(engine, file)`, `te::EditFileOperations(edit).writeToFile(file, false)`, `edit.resetChangedStatus()`, `hasChangedSinceSaved()`, `undo()/redo()`, `ensureNumberOfAudioTracks(n)`, `deleteTrack(t)`, `getMasterPluginList()`, `getMasterVolumePlugin()`, `getAutomationRecordManager().setWritingAutomation(bool)`, `tempoSequence.getTempo(0)->setBpm()`, `toBeats/toTime/toBarsAndBeats`, `getTimeSigAt(t).numerator`
- Tracks: `te::getAudioTracks(edit)`, `track.insertWaveClip(name, file, ClipPosition, false)`, `insertMIDIClip(name, TimeRange, nullptr)`, `splitClip(clip, time)`, `getClips()`, `pluginList`, `getVolumePlugin()` (`volParam`, `panParam`, `setVolumeDb`, `setPan`), `getLevelMeterPlugin()->measurer`, `setMute/setSolo`, `isMuted(false)/isSolo(false)`
- Clips: `setStart(t, preserveSync, keepLength)`, `setEnd(t, preserveSync)`, `getMaximumLength()`, `moveTo(clipOwner)`, `getEditTimeRange()`, `getOffsetInBeats()`, `getLengthInBeats()`, `getContentBeatAtTime(t)`. `MidiClip::getSequence()`: `addNote(pitch, BeatPosition, BeatDuration, vel, 0, um)`, `removeNote`, `getNoteFor(valueTree)`. `MidiNote::setStartAndLength/setNoteNumber`
- Plugins: `edit.getPluginCache().createNewPlugin(XxxPlugin::xmlTypeName, {})` or `createNewPlugin(te::ExternalPlugin::xmlTypeName, desc)`; `pluginList.insertPlugin(ptr, index, nullptr)` (-1 appends); `plugin->deleteFromParent()`, `isSynth()`, `windowState->showWindowExplicitly()`. Built-ins: FourOsc, Sampler, Equaliser, Compressor, Reverb, Delay, Chorus, Phaser, LowPass, AuxSend/AuxReturn (`busNumber`, `setGainDb`, `gain`). Indexing `PluginList::getPlugins()` gives a `Ptr`, so call `.get()`.
- Automation: `param->getCurve()` (time-based): `getNumPoints`, `getPointPosition(i)` (convert with `te::toTime(pos, edit.tempoSequence)`), `getPointValue`, `addPoint(te::EditPosition(t), v, 0.0f, um)`, `movePoint(i, EditPosition, v, std::nullopt, false, um)` (returns the new index), `removePoint`, `clear`. Values map to 0..1 with `param->valueRange.convertTo0to1/convertFrom0to1`.
- MIDI input: `engine.getDeviceManager().getMidiInDevices()`, then `edit.getAllInputDevices()` and `instance->setTarget(trackID, true, nullptr, 0)`.
- Plugin scanning: `juce::PluginListComponent` with `engine.getPluginManager()`; the engine saves the scan results itself.
