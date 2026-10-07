# OpenDAW

A free, open-source digital audio workstation for Windows, built on
[JUCE](https://juce.com) and [Tracktion Engine](https://github.com/Tracktion/tracktion_engine).

**Goal:** the fastest free DAW for writing beats and songs. That means Studio One-style drag-and-drop,
an FL Studio-grade piano roll and step sequencer, an Ableton-style clip launcher, and Pro Tools-level
recording and mixing.

> Status: early development (Milestone 2).

## Downloading

**[Download the latest build](../../releases/download/latest/OpenDAW.exe)** — this link always points to
the newest build, so you can re-download the same URL after every change instead of hunting for a new
Actions run. It's updated automatically on every push to `main` or a `milestone-*` branch; see the
[latest release](../../releases/tag/latest) for which commit it was built from.
Windows SmartScreen may warn about an unsigned app; choose **More info → Run anyway**.

Alternatively, every CI run also keeps its own build under the [Actions tab](../../actions) →
pick a run → **OpenDAW-windows** under *Artifacts*, if you need a specific past build.

## Using OpenDAW

**Getting sound in**
- Drag audio files (WAV, MP3, FLAC, OGG, AIFF) from Explorer onto a track; they land at the beat under the mouse
- `+ Instrument` (Ctrl+I) adds a track with the built-in 4OSC synth
- Double-click empty space on a track to create a one-bar MIDI clip and open it in the piano roll
- MIDI keyboards play whichever track is selected (click a track name or clip to select it)

**Arranging**
- Drag a clip to move it in time or onto another track
- Hover a clip to show its trim handles, then drag the left/right handle to trim it
- Ctrl+E splits the selected clip at the playhead; F zooms to fit the whole project
- Moves and trims snap to beats while **Snap** is on
- Click the ruler or empty space to move the playhead

**Piano roll** (double-click a MIDI clip)
- Click to draw a note; drag while drawing to set its length (new notes reuse the last length)
- Drag a note to move it, drag its right edge to resize it
- Right-click or double-click a note to delete it
- Choose the grid size (1/4 to 1/32) in the editor header; Escape closes the editor

**Tracks and plugins** (right-click a track name)
- *Instrument*: 4OSC or any scanned VST3 instrument
- *Add Effect*: built-in EQ, Compressor, Reverb, Delay, Chorus, Phaser, Low Pass, or any scanned VST3 effect
- *Plugins on this Track*: open a VST3's editor, bypass, or remove
- **M** / **S** buttons mute and solo the track
- Find your VST3s with **Plugins → Scan for Plugins** (only needed once)

**Navigation**

| Action | Mouse / Key |
|---|---|
| Zoom | Ctrl + wheel |
| Scroll horizontally | Shift + wheel |
| Scroll tracks / pitches | Wheel |

**Shortcuts**

| Key | Action |
|---|---|
| Space | Play / pause |
| Enter | Stop and return to start |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Delete | Delete selected clip or note |
| Ctrl+E | Split selected clip at playhead |
| F | Zoom to fit |
| Ctrl+T / Ctrl+I | Add track / instrument track |
| Ctrl+N / Ctrl+O | New / open project |
| Ctrl+S / Ctrl+Shift+S | Save / save as |

Projects are saved as `.opendaw` files. They store the full path to each audio file, so keep your audio where it is.

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

- [x] **M0: Playable core.** Transport, tempo, and drag-and-drop audio files onto tracks
- [x] **M1: Arrangement.** Move and trim clips, multiple tracks, undo/redo, save and load projects
- [x] **M2: MIDI.** MIDI tracks, piano roll, VST3 instruments and effects, MIDI keyboard input
- [ ] **M3: Mixing.** Mixer view, sends and buses, automation, built-in EQ, compressor and reverb
- [ ] **M4: Beat making.** Step sequencer and pattern workflow
- [ ] **M5: Performance.** Clip launcher
- [ ] **M6: Recording.** Multitrack recording, comping, and punch in/out

## License

OpenDAW is licensed under the [GNU GPL v3](LICENSE). Tracktion Engine and JUCE are used under their GPLv3 and AGPLv3 options respectively.
