# Elevate

A paraphonic analog-style synthesizer (VST3), built with JUCE. Elevate is an original
instrument whose feature set is modelled on the controls described in the UAD Anthem manual.
It is not affiliated with Universal Audio, and contains no UAD code, samples or artwork.

## Features
- **2 oscillators** (each with up to **11-voice unison** and its own unison detune) with continuous Triangle → Saw → Square → Pulse shape, octave/coarse/fine tune, shape mod (LFO or filter env)
- **Sub oscillator** (-1 / -2 oct), **noise** (pink/white), **Osc 2 Warp** (Sync / Ring, up to 36 st) with LFO/F.ENV mod, pitch mod
- **Filter**: 24 dB ladder low-pass with Drive, Resonance, Growl; Env / LFO / Key tracking; 12 dB high-pass with "To HP" follow
- **Two ADSR envelopes** (Filter, Amp) with velocity scaling
- **LFO**: triangle / square / S&H / smooth random, Tilt, tempo Sync, Retrig
- **Voice**: Para (4 voices, shared filter/envelopes), Mono, Unison with Detune; Legato, Glide, mod wheel destinations (Vibrato / Cutoff / LFO Rate), bend range, master tune
- **Effects**: Chorus, Mod FX (Phaser, Flange +/-, Warble), Space FX (tempo-synced Echo, Spring, Hall)
- **16-step sequencer** with Gate / Pitch / Velocity lanes, independent lane lengths (polyrhythm), legato ties, octave per step, swing, gate scale, 5 directions, retrigger or host-locked
- **Mod Sequencer (Sequencer 2)**: a second 16-step sequencer that drives any control (73 destinations) instead of playing notes. Each step's pitch value (±12 = full scale) is the modulation level, held until the next step; plus Amount (long-throw slider for fine control) and Slew
- **Randomizers** on both sequencers (all lanes or just the visible lane on Sequencer 1; full-range steps on the Mod Sequencer)
- Default MIDI CC map, 9 factory presets

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
The VST3 is written to `build/Elevate_artefacts/Release/VST3/Elevate.vst3`.
Linux needs: `libasound2-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxcomposite-dev libgl1-mesa-dev libcurl4-openssl-dev libfreetype-dev`.
GitHub Actions builds Windows, macOS and Linux binaries on every push.

JUCE is fetched automatically (see JUCE licensing for distribution terms).
