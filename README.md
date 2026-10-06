# OpenDAW

A free, open-source digital audio workstation for Windows, built on
[JUCE](https://juce.com) and [Tracktion Engine](https://github.com/Tracktion/tracktion_engine).

**Goal:** the fastest free DAW for writing beats and songs. That means Studio One-style drag-and-drop,
an FL Studio-grade piano roll and step sequencer, an Ableton-style clip launcher, and Pro Tools-level
recording and mixing.

> Status: early development (Milestone 0).

## Building

Requirements:
- Windows 10/11 (x64)
- [Visual Studio 2022 or newer](https://visualstudio.microsoft.com/vs/community/) with the **Desktop development with C++** workload (this includes CMake)

```sh
git clone https://github.com/<you>/OpenDAW.git
cd OpenDAW
git -c url."https://github.com/".insteadOf="git@github.com:" submodule update --init --recursive
cmake --preset vs
cmake --build --preset release
```

The app is built to `build/OpenDAW_artefacts/Release/OpenDAW.exe`.
You can also open the `.sln`/`.slnx` file in `build/` in Visual Studio to build and debug.

## Roadmap

- [ ] **M0: Playable core.** Transport, tempo, and drag-and-drop audio files onto tracks
- [ ] **M1: Arrangement.** Move and trim clips, multiple tracks, undo/redo, save and load projects
- [ ] **M2: MIDI.** MIDI tracks, piano roll, VST3 instruments
- [ ] **M3: Mixing.** Mixer view, sends and buses, automation, built-in EQ, compressor and reverb
- [ ] **M4: Beat making.** Step sequencer and pattern workflow
- [ ] **M5: Performance.** Clip launcher
- [ ] **M6: Recording.** Multitrack recording, comping, and punch in/out

## License

OpenDAW is licensed under the [GNU GPL v3](LICENSE). Tracktion Engine and JUCE are used under their GPLv3 and AGPLv3 options respectively.
