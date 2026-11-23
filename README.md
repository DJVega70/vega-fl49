# VEGA FL49

A DAW built around the **Novation FLkey 2 49**, by Vega Productions. The keyboard's faders, knobs and pads are on screen and in sync, and the clip launcher is the main view. It runs on the same engine as Vega Productions DAW (the open-source **Tracktion Engine** and JUCE), so recording, plug-ins, the mixer, automation, stems and `.vegaproj` projects all work the same.

![VEGA FL49](docs/screenshot.png)

*Unofficial: VEGA FL49 is not made by or affiliated with Novation / Focusrite.*

## Download (Windows)
- **Installer (recommended):** https://github.com/DJVega70/vega-fl49/releases/latest/download/VEGA-FL49-Setup.exe. Run it to install to Program Files with Start menu / desktop shortcuts, `.vegaproj` file opening, and an uninstaller in Settings > Apps. Run a newer Setup to update.
- **Portable:** https://github.com/DJVega70/vega-fl49/releases/latest/download/VEGA-FL49-Windows.zip. Unzip anywhere and run `VEGA FL49.exe`. Keep `VegaStemHelper.exe` and `VegaStemHelperAVX2.exe` in the same folder: they run the AI stem separation.

## The screen
- **Top row:** the menus, then the song **KEY** (it says **SCALE LOCK ON** when Snap or Filter is on), **HARDWARE** (FLkey 2 49 SYNC OK when both of the keyboard's ports are found) and **SCENE LAUNCH** quantise.
- **Transport bar:** play / stop / record / loop / click, tempo, time signature and snap.
- **Clip launcher** (VEGA FL49 opens here; **Shift+Tab** switches to the arrangement):
  - Columns are tracks (`1:DRUMS`, `2:PERC` ...) in their track colours.
  - A stopped clip is a dark tint of its colour with a ring. Click the ring to launch it.
  - A playing clip lights up and its ring fills as the loop plays. A queued clip blinks.
  - **Scenes** on the right: click a scene to launch the row. The last one you launched is marked **ACTIVE**.
- **FLkey hardware strip** along the bottom (**H** shows / hides it). It mirrors the keyboard live, and every part also works with the mouse:
  - **Track ◀ ▶** move the 8-track bank (the faders, knobs and pads follow).
  - **MUTE / SOLO / ARM** chooses what the buttons under the faders do.
  - **Pitch** and **Modulation** wheels play into the selected instrument. The pitch wheel springs back.
  - **8 faders + MASTER**, each with a level meter and its track's name tag. Click a name to select the track; double-click a fader for 0 dB.
  - **8 knobs** with their values on top: **PAN**, **SENDS** (click again for A-E) or **PLUGIN** (the selected track's plug-in macros).
  - **16 pads:** **DRUMS**, **CLIPS** (a 2 x 8 launcher, ▲ ▼ change scenes) or **SCENES**. **■** stops all clips and **▶** plays / stops.
- **Arrangement:** the FLkey side panel docks on the right (**F**), with knobs, pads, keys (scale & chords) and faders.

## Roadmap
- **Stage 1 (this build):** the new look: top row status, clip launcher, hardware strip, arrangement with the FLkey dock.
- **Stage 2:** Chord Assist & Arpeggiator: a chord grid in the key, voicings / inversions, tensions (7th, 9th, add11, add13), and a synced arpeggiator.
- **Stage 3:** Device Rack: each track's plug-ins as a chain of cards with live visualizers, plus performance FX (filter sweep, beat repeat, reverb wash, delay throw, stutter, tape stop, bitcrush).

First run: **Options > Audio / MIDI settings** (pick ASIO / WASAPI on Windows, CoreAudio on Mac, and enable your MIDI keyboard), then open the **PLUGINS** panel (button at the top right, or press **B**). The DAW scans your standard VST3 folder (and AU on Mac) by itself on first launch and picks up newly installed plug-ins every time it starts. Use **FOLDERS** if you keep .vst3 files somewhere else.

**Mixer (Tab)**, a console in the Vega plug-ins' look (same knobs, square buttons, faders and LED meters). Each channel strip, top to bottom:
- **Input**: what the track records and plays live.
- **Console**: a VU meter plus the DJ section's **HI / MID / LO** isolator EQ.
  - The crossovers are at 250 Hz and 2.5 kHz.
  - 12 o'clock is flat, the top of each knob is +6 dB, and turning a knob fully left **kills** that band (its label turns red).
- **Tape**: Off, **15 ips** (warm: softer top, a little more low end) or **30 ips** (clean, a little glue). It keeps the level the same.
- **Inserts**: each plug-in on a black display; click to open, right-click to bypass, move or remove; **+** adds one. Instrument tracks show their instrument first.
- **Sends** to buses with a level knob, and **+ Send**.
- **Output**: Master or a bus.
- **READ / WRITE / OFF** automation, **PAN** and the one-knob **FILTER** (left low-pass, right high-pass, centre off).
- **Rec**, **S** (solo) and **M** (mute), the fader with stereo LED meters, and the name with the track colour. Click the name to select the track.
- Double-click any knob or fader to reset it. The panel's **EDIT** key hides the inserts and sends.
- **Master section**: a full console channel strip on the master bus, in the order input → filters → EQ → gate → compressor → output.
  - **Filters:** HP (16–500 Hz, 18 dB/oct) and LP (3–22 kHz).
  - **4-band EQ** with an **EQ** in/out switch:
    - **HF** (1.5–22 kHz, shelf or bell);
    - **HMF** (0.6–7 kHz, with Q);
    - **LMF** (0.2–2 kHz, with Q);
    - **LF** (40–600 Hz, shelf or bell).
  - **Compressor:** ratio 1:1 to ∞:1, threshold, release, fast attack, peak/RMS detection and automatic make-up gain.
  - **Gate / expander:** range, threshold, release, hold, expand and fast attack.
  - **Gain-reduction LEDs** for the compressor and the gate.
  - **Input and output trims**, big output meters with peak hold, a polarity switch and **CUT**.
  - **The master fader.**
  - Everything is saved with the project. Double-click a knob to reset it.
- **Each fader** has a scale and a level meter. Under the faders are two rows of lit pads: **Solo** and **Mute** (lit = playing).
- **The left column** is a console-style control unit, the same size as the master section:
  - **Soft keys** (labels shown on the screen):
    - **VU METERS**: click to reset the peak hold.
    - **EDIT**: shows or hides each channel's instrument, inserts and sends.
    - **SHIFT**: turns the pads into **Arm / Select**.
    - **LEARN**: MIDI learn.
  - **Screen:** two backlit VU meters on the master output, with warm-lit blue faces, moving needles and a red zone. Under each meter are the **current** and **max** levels. The top line shows the last control you touched.
  - **4 encoders:** their jobs are shown on the bottom bar of the screen. Double-click one to reset it.
    - Normally they control the selected track's **Volume / Pan / Filter**, plus the **VU reference** level.
    - With **5-8** lit they control **Hi / Mid / Low** and the **Master** volume.
  - **CHANNEL knob:** turn it to select tracks.
  - **Key groups:**
    - **MODE** switches the arrow keys to zoom.
    - **◀ ▶** shift the mixer by one channel; **BANK ◀ ▶** move 8 channels.
    - **360° LAYER** makes ◀◀ / ▶▶ nudge the selected clip by one grid step.
    - **SCRUB** makes the jog wheel play the audio as you turn it.
  - **Arrow cluster:** selects tracks and moves the playhead by a bar. The centre dot drops or removes a marker.
  - **Jog wheel:** moves the playhead, or scrubs with SCRUB lit.
  - **Function row:**
    - **OFF / READ / WRT** set the automation mode.
    - **CYCLE** is loop, **CLICK** is the metronome, **DROP** is punch-in.
    - **SHIFT** makes ◀◀ / ▶▶ jump between markers.
  - **Transport:** ◀◀ ▶▶ (hold to keep going) ■ ▶ ●. Play lights green and record lights red.

**Stage 2**
- **Automation:** press **A** on a track to open its lane and pick any parameter (volume, pan, the DJ EQ, plug-in knobs). Click to add points, drag to move them, and double-click or right-click to delete. Turn on **WRITE** in the transport, then press play and move a fader to record automation.
- **MIDI learn:** press **LEARN**, move a control on screen, then move a knob on your controller. **Options → MIDI learn mappings** lists and removes them. Mappings are saved with the project.
- **Mackie Control (MCU):** **Options → Mackie Control** sets up an X-Touch, iCON, QCon, FaderPort or similar surface in MCU mode. Its faders, V-pots, transport, banking and display follow the mixer.
- **Elastic audio (warp markers):** right-click an audio clip and turn on elastic audio. Ctrl/Cmd-click inside the clip to add a marker, drag a marker to stretch the audio around it, and double-click one to remove it.
- **Freeze:** right-click a track header and choose Freeze to render its instrument and plug-ins to audio and save CPU.

**Stage 3**
- **Routing matrix (Track → Routing matrix...):**
  - Every track and bus is listed down the side.
  - **OUTPUT** columns route each track to the Master or to a bus.
  - **SENDS** columns add or remove sends to each bus (−6 dB to start); drag up or down on a send to change its level.

**Stage 4 - full workflow**
- **Clip launcher (Shift+Tab, or SESSION):** tracks across, scenes down.
  - Click a clip's triangle to launch it, or a scene's triangle to launch the row. Clips start on the next bar by default; change this with the **Launch** menu, or per clip.
  - Double-click an empty slot on an instrument track for a new MIDI clip. You can also drop audio or MIDI files on a slot, or right-click an arrangement clip and choose **Copy to the clip launcher**.
  - Drag a clip to move it (Alt-drag copies it).
  - Right-click a clip to set its **follow action** (next, previous, any, play again, stop, back to arrangement...) and after how many loops it happens.
  - Right-click an empty slot and choose **Record into this slot**: the new clip starts looping when you stop recording.
  - A launched clip takes its track over from the arrangement. **BACK TO ARRANGEMENT** hands it back.
- **Comping:** turn on LOOP and record. Every pass becomes a take in one clip.
  - Click **TK** on the track header to show the take lanes.
  - Click a take to use it for the whole clip, or drag across part of a take to comp just that part (the clip is split with tiny crossfades).
  - Right-click the clip > **Takes** to keep only the current take or put every take on its own track.
- **Export (File > Export mixdown / Export stems, Ctrl+Shift+E):**
  - Formats: WAV, AIFF, FLAC, OGG, or MP3 at 320 kbps (MP3 needs the free LAME encoder; put lame.exe next to the DAW).
  - Choose 16 / 24 / 32-bit and 44.1-96 kHz.
  - Normalise to a peak level, or to -14 LUFS (streaming) or -9 LUFS (club), with true peaks kept under -1 dBTP.
  - Stems are one file per track, bus and folder, all starting at the same time.
- **Bounce in place / Consolidate (right-click a track header):** works on the loop range, or the whole track.
  - Bounce renders the track with its plug-ins to a new audio track and mutes the original.
  - Consolidate joins clips into one.
- **Folders:** Track > Add folder, or right-click a header > Folder.
  - A folder is a bus that its tracks play through, so its fader and inserts work on the whole group.
  - Collapse it with the arrow. Moving the folder moves its tracks too.
- **Tempo and time-signature changes:** right-click the ruler. The bar lines follow the new signature.
- **Markers:** press **M** or right-click the ruler. Double-click a marker to rename it. **Edit > Markers & tempo list** shows everything, with GO buttons.
- **Piano roll controller lane:** the menu above the bottom lane switches it from velocity to mod wheel, expression, sustain, volume, pan, cutoff, resonance, pitch bend or aftertouch. Draw with the mouse; right-click erases.
- **Step sequencer:** right-click an empty spot on an instrument track > **New step-sequencer clip**. Its rows are General MIDI drums, so it plays Vega Productions Drums directly. Choose 8-64 steps and 1/8-1/32 rate; Shift-click adds a soft hit.
- **Other additions:**
  - **Duplicate track.**
  - **File > Open recent.**
  - **Autosave** every 2 minutes. If the DAW didn't close normally, it offers to recover your work the next time it starts.

**Using plug-ins**
- **Load:** in the PLUGINS panel, double-click a plug-in or drag it onto a track or mixer strip. Its window opens right away. An instrument goes on the selected instrument track, or on a new one. An effect goes on the selected track.
- **Reopen:** every track header has an **INST** button that opens its instrument and an **FX** button that lists every plug-in on the track; click one to open it. In the mixer, click the instrument name or an insert slot.
- **Window position:** plug-in windows stay on top of the DAW, so they never get lost behind it.

## Everything from Vega Productions DAW
**Audio engine** — 32-bit floating-point processing, multi-core graph, plugin delay compensation (Tracktion Engine). ASIO (when available at build time), WASAPI and DirectSound on Windows; CoreAudio on Mac.

**Arrange**
- Audio, instrument and bus tracks; record audio and MIDI (arm **R**, choose inputs with **IN**, input monitoring), loop recording, metronome.
- Non-destructive clips: move (also between tracks), trim either edge, fade in/out handles, split at playhead, duplicate (Alt-drag or Ctrl+D), mute, rename, clip gain, reverse, loop.
- Audio clips: **Warp to project tempo** (time-stretch) and **pitch shift** ±12 semitones, **slice at transients**.
- Drag audio (WAV/AIFF/FLAC/OGG/MP3) or MIDI files straight onto the timeline.
- Snap like FL Studio: **Line** (the default) snaps clips, audio included, to the grid you see, finer as you zoom in; or Bar / Beat / 1/8 / 1/16 / Off. Hold Shift while dragging to move freely. Zoom (Ctrl+wheel), loop range on the ruler, tempo and time signature, undo/redo.

**Piano roll** — add / move / resize / delete notes, velocity lane, grid incl. triplets, quantise, note audition.

**Tape machine (Vega Tape 2T)** - the reel-to-reel on the mixer's control panel is a tape machine on the master bus. Its reels turn with the song, and dragging a reel moves the playhead. **DRIVE** sets how hard the mix hits the tape, **IPS** sets the speed (7.5 darker and fatter, 15 warm, 30 clean), and **CAL** trims the output. **On / Bypass** switch it in and out; it starts bypassed. **PRESET** has five Vega presets, and **SAVE / RENAME / DELETE** manage your own.

**Floating windows** — the mixer and the piano roll each have a **DETACH** button (also in the Track menu). Detached, they float in their own window, which you can put on a second screen, and stay above the DAW while you work. **DOCK** puts them back. Closing a floating window just hides it: Tab brings the mixer back, and double-clicking a clip brings the piano roll back. The DAW remembers which views float and where.

**Mixer** — channel strips with inserts (VST3/AU or built-ins: EQ, compressor, reverb, delay, chorus, phaser, filter, pitch shift), plugin bypass/re-order, **aux sends** to buses, **output routing** of tracks into buses, **sidechain inputs** for plugins that support them, pan, fader, meters, mute/solo, master.

**Instruments** — every instrument track can host any VST3/AU synth (Vega Multisynth, Workstation, Drums, Sampler, Chords…) or the built-in 4-Osc synth / sampler. Your MIDI keyboard plays whichever instrument track is selected.

**Files** — projects saved as `.vegaproj`; export the mixdown (or just the loop range) to 24-bit WAV.

## The FLkey 2 49 in depth
- **Setup:** plug in the FLkey 2 49 and start VEGA FL49. It finds both of the keyboard's ports by itself:
  - the **MIDI** port (keys, pads, wheels), shown as **KEYS: SYNCED**;
  - the **DAW** port (HUI: transport, faders, encoders, buttons), shown as **DAW: ACTIVE**.
- Leave the FLkey's own Scale and Chord modes **off**. VEGA FL49 does scales and chords on the computer, so they follow the song's key.
- **F** shows or hides the panel (also in **Track > Show FLkey 2 49 panel**).

**The FLkey panel** mirrors the hardware live, and you can also use it with the mouse:
- **KNOBS:** what the 8 encoders do.
  - **PAN** or **SENDS** (click SENDS again to step through A-E) for the 8 tracks the faders show.
  - **PLUGIN** turns the encoders into **plug-in macros**: the selected track's instrument (◀ ▶ picks an effect instead), 8 parameters at a time (**1-8** pages through them).
  - **OPEN** opens that plug-in.
  - Knob turns are recorded when automation **WRITE** is on.
- **PADS:** what the 16 pads do. Put the FLkey's pads in **Drum** mode.
  - **DRUMS:** the pads play the selected instrument (General MIDI notes 36-51, so they play Vega Productions Drums).
  - **CLIPS:** a 2 x 8 clip launcher. Columns are the 8 tracks the faders show; rows are two scenes (▲ ▼ move them). A pad launches its clip, or stops it if it's playing. A pad on an empty slot stops that track. The panel shows each pad in its track's colour, green while playing, blinking while queued.
  - **SCENES:** the top row launches scenes, the bottom row stops tracks.
  - **TEACH PADS:** if your pads send different notes, press it and hit the 16 pads, top row first, left to right. **DEFAULT** goes back to channel 10, notes 36-51.
- **KEYS:** the song key (root and scale) and two switches. The settings are saved with the project.
  - **NOTES:** **OFF**, **SNAP** (every key plays the nearest note in the key) or **FILTER** (keys outside the key don't play).
  - **CHORDS:** one key plays a chord built from the key: **TRIAD**, **7TH**, **9TH** or **SOUL** (root, 5th, 7th, 9th with the 3rd on top, an open Soulful House voicing).
  - What you record is what you heard: snapped notes and whole chords end up in the clip.
- **FADERS:** the 8 tracks the hardware faders control right now (they follow the bank buttons). Drag one to set its volume; click to select the track. Dots show mute (amber) and solo (blue).

**Still on the FLkey's DAW port (HUI):** faders 1-8 set track volume, fader buttons select tracks, DAW-mode pads mute / solo, the transport buttons work, and the bank buttons move through the tracks. See *Controllers: Novation FLkey 2* below.

**Not possible yet:** Novation doesn't publish how to set the FLkey 2's pad colours, screen text or its built-in Scale mode from a DAW other than FL Studio. So the pads light the way the keyboard's Drum mode lights them, and the panel on screen shows the clip colours and states instead.

## Controllers: Novation FLkey 2 (and other HUI surfaces)
The DAW has a built-in **HUI controller script**, the protocol the FLkey 2 uses for DAWs other than FL Studio.

1. Plug in the FLkey 2 49 and start the DAW. It connects automatically to the keyboard's **DAW port**
   (`MIDIIN2 / MIDIOUT2 (FLkey 2 49)` on Windows, `FLkey 2 49 DAW Out / DAW In` on Mac).
   Check **Options > Controllers (FLkey 2 / HUI)** — it says CONNECTED, and every button/fader you touch appears in the MIDI monitor.
2. The keys, wheels and drum pads (in Drum mode) use the normal **FLkey 2 49 MIDI** port and play whichever instrument track is selected —
   pads send General-MIDI drum notes, so they play Vega Productions Drums directly.

| FLkey 2 control | Vega Productions DAW |
|---|---|
| Faders 1–8 | Volume of 8 tracks (motorless faders jump to the value) |
| Fader buttons | Select track (MIDI keyboard follows the selection) |
| Encoders | Pan — or Send A–E level in Sends mode (turning up a send creates it to that bus) |
| Pads (DAW mode) | Mute / Solo |
| Bank / Channel ◀ ▶ | Move the 8-track window / one track |
| Play, Stop, Record, Loop | Transport (Stop when stopped = return to start) |
| Track/cursor buttons | Previous / next track, playhead by a beat |

Feedback goes back to the controller: fader values, mute/solo/select/arm lights, encoder positions, 4-letter track names and transport lights.

## Controllers: Akai APC Key 25 mk2
The DAW has a built-in script for the APC Key 25 mk2. It is laid out like Ableton's script, and the pads drive the clip launcher.

1. Plug it in. The DAW connects to the APC's **control port** by itself: `MIDIIN2 / MIDIOUT2 (APC Key 25 mk2)` on Windows, or `APC Key 25 mk2 Control` on Mac. To check the connection or pick the ports yourself, use **Options > Akai APC Key 25 mk2**.
2. The keys, Sustain and Oct -/+ use the APC's main port and play whichever instrument track is selected.

| APC Key 25 mk2 | Vega Productions DAW |
|---|---|
| 5 x 8 pads | Clip launcher slots. The red box in the SESSION view shows which ones. A press launches a clip. An empty slot on an instrument track (or an armed audio track) **records a new clip** there, with its pad blinking red; press it again to stop and the new clip starts looping. Other empty slots stop that track. The pads light up in the track colour, bright green while playing, and blink while queued. |
| Shift + pad | Select the clip. Shift + double-tap opens it in the piano roll or step editor. |
| Scene buttons (Clip Stop ... Select) | Launch the scene row |
| Shift + scene button | Pick the soft-key mode for the 8 buttons under the pads: **Clip Stop / Solo / Mute / Rec Arm / Select** |
| ▲ ▼ ◀ ▶ Volume Pan Send Device | The soft-key action on the 8 tracks shown. In Mute mode a lit button means the track is playing. |
| Shift + ▲ ▼ ◀ ▶ | Move the 8 x 5 box |
| Shift + Volume / Pan / Send / Device | What the knobs control: track volume, pan, the send to the first bus, or the first 8 parameters of the selected track's plug-in |
| K1-K8 | The 8 tracks shown (or the plug-in). Knob moves are recorded when WRITE is on. |
| Stop All Clips | Stop the launcher. Shift + Stop All Clips goes back to the arrangement. |
| ▶/II, ● | Play / stop and record. Shift + Play returns to the start. |
| Shift + ● | **Drum pads** on / off. The 40 pads play the selected instrument track (GM drum notes starting at 36 on the bottom-left pad, so they play Vega Productions Drums). |

## Building
```
git clone https://github.com/Tracktion/tracktion_engine.git te   # see TE_COMMIT in the workflow
git -C te submodule update --init modules/juce
cmake -B build -DTE_DIR=$PWD/te -DVEGA_TESTS=ON
cmake --build build --config Release --target VegaDAW VegaDAWTest
```

## License
GPLv3 (see `LICENSE`), because Tracktion Engine is used under its GPLv3 license (`TRACKTION_LICENSE.md`). Free for your own use; selling it would need a commercial Tracktion Engine license.
