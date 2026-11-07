# kalabash modules

These are five Ableton devices built from Mutable Instruments' open-source Eurorack firmware. The VST3/AU plugins run Émilie Gillet's original DSP code, compiled for desktop. They don't approximate it. The Max for Live device is a line-by-line JavaScript port of the Marbles algorithms.

| Device | Based on | Type | Format |
|---|---|---|---|
| **KNOTWORK** | Plaits | Instrument (polyphonic, 1–8 voices) | VST3 / AU |
| **ALLOY** | Elements | Instrument (mono, like the module) | VST3 / AU |
| **FOGBANK** | Clouds | Audio effect | VST3 / AU |
| **ABACUS** | Beads | Audio effect | VST3 / AU |
| **DICEBAG** | Marbles | MIDI effect, sequencer and modulator | Max for Live (.amxd) |

The names are new because the eurorack README asks derivative works not to use the Mutable Instruments name or the module names. Each device credits the original in its UI.

## Install (Mac)

Push this repo to GitHub. The **Build macOS installer** action (`.github/workflows/build-mac.yml`) builds universal (Apple Silicon and Intel) VST3 and AU plugins, ad-hoc signs them, runs `auval`, and produces `kalabash-modules-<version>.pkg`. Push a tag like `v1.0.0` to get a GitHub Release with the installer attached.

The installer puts:
- VST3 plugins in `/Library/Audio/Plug-Ins/VST3`
- AU plugins in `/Library/Audio/Plug-Ins/Components`
- DICEBAG in `~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/kalabash/DICEBAG`

The package is unsigned. To install it, right-click it and choose Open the first time. In Live, rescan plugins if they don't appear.

## Per-device notes

### Tuning (KNOTWORK and ALLOY)
- **TUNED** (on by default) snaps FREQUENCY/COARSE to semitones, so a MIDI keyboard plays in tune. FINE is in cents and **A4** sets concert pitch (415–466 Hz).
- Measured: KNOTWORK is 0.0 cents out from C2 to A6. ALLOY's modal mode is within about ±2.5 cents. Its string modes get a delay-loop correction in TUNED mode, which brings plucked strings to within about ±5 cents.

### KNOTWORK (Plaits)
- The UI shows all 24 models in three banks: the original 16 plus the 8 added in firmware 1.2 (VA+filter, phase distortion, three six-op FM banks, wave terrain, string machine, chiptune).
- The hint under HARMONICS, TIMBRE and MORPH changes with the selected model.
- The three attenuverters set how much of the internal decay envelope reaches TIMBRE, FREQUENCY and MORPH, as they do on the module when TRIG is patched.
- The module's hidden LPG settings (hold the button and turn the knobs) get their own DECAY and COLOUR section here.
- **Amp mode:**
  - *Pluck*: a note strikes the LPG, like patching TRIG.
  - *Gate*: the LPG stays open while the key is held, like patching LEVEL.
  - *Drone*: nothing is patched, so it plays continuously.
- It runs at the module's real 47,872 Hz clock, so tuning is exact. A440 measures 440.00 Hz.

### ALLOY (Elements)
- The panel is split into exciter and resonator halves.
- The module's attenuverters become **MOD** depth knobs driven by a selectable source: velocity, mod wheel, aftertouch, LFO, or a new random value per note.
- **Resonator** offers modal, string and strings. **OMINOUS** is the module's hidden easter-egg mode.
- **PLAY** works like the panel button.
- The sidechain input acts as Elements' external audio input: left goes to strike, right to blow. Set its level with EXT IN.

### FOGBANK (Clouds)
- Granular, stretch, looping delay and spectral modes, each with the module's 4 quality and buffer options.
- The hints under the knobs change with the mode.
- The single BLEND knob is split into four knobs: dry/wet, spread, feedback and reverb.
- **TRIG SYNC** fires grain triggers in time with Live's tempo.
- *Bug fix:* the current upstream Clouds code never re-arms its stretch-mode windows, so that mode is silent. `third_party/eurorack/clouds/dsp/window.h` fixes this (marked "kalabash fix").

### ABACUS (Beads)
- The four grain parameters each have an attenurandomizer. Left gives peaky random, right gives smooth drift, matching the module's behaviour with nothing patched.
- **SEED** plus **LATCH** cover the module's gated and latched modes. **SEED CLOCK** gives the clocked mode, synced to Live.
- Four quality modes:
  - Digital 48k
  - Cloudy 32k
  - Clean tape 24k
  - Dirty tape 24k
- **MONO 2x** doubles the buffer length.
- **SYNTH** is the wavetable mode the module enters when nothing is patched.
- **SIZE** fully clockwise switches to delay mode.

### DICEBAG (Marbles)
Drop it on a MIDI track in front of an instrument.

- **t section:**
  - RATE: free-running, 120 BPM at noon.
  - Live sync: uses the Division menu.
  - JITTER, BIAS, GATE length and random.
  - Seven t models: coin toss, random ratio and drums, plus the hidden models (independent coins, fixed divider, three states, Markov).
- **Notes:** lane *k* plays a note when gate t*k* fires, at the pitch of X*k*. ROOT sets the note that 0 V maps to.
- **DÉJÀ VU / LENGTH**, with separate off/on/lock switches for t and X, like the module.
- **X section:** SPREAD, BIAS and STEPS, with six scales (major, minor, pentatonic, pelog, raag Bhairav, raag Shri).
  - The diversity modes are *same*, *X2 vs*, and *tilt*.
  - The X clock sets which t gate clocks the X outputs.
  - **EXT** feeds incoming MIDI notes into the déjà vu loop, as external CV does on the module.
- **Y:** the slow, smooth random output. It is never affected by déjà vu.
- **Modulation:** X1, X2, X3 and Y can each be mapped like Ableton's LFO.
  1. Click **MAP**.
  2. Click any parameter in Live, including rack macros and devices inside racks.
  3. Set the range with MIN and MAX. **ON** bypasses that output, and **x** clears the mapping.

  Mappings are saved with the Live set.

> The device loads its three `.js` files from its own folder. Once it opens fine, you can click **Freeze** in the Max editor to embed them.

## Building on Linux/Mac for development

```
cmake -B build -DCMAKE_BUILD_TYPE=Release        # fetches JUCE 8.0.4
cmake --build build -j
python3 marbles-m4l/build-tools/make_amxd.py      # builds marbles-m4l/build/DICEBAG.amxd
```

`tools/` contains offline DSP tests (`test_plaits.cpp`, …) and a Node test for the Marbles port.

## Licences
- The DSP code is © Émilie Gillet, MIT licence. See `third_party/eurorack`.
- Using the Mutable Instruments name, or the module names, for derivative products is not permitted.
- The plugin shells and UI use JUCE.
