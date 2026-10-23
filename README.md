# Life Pedal Sim (VST3)

Unofficial emulation of the Sunn O))) x EarthQuaker Devices *Life Pedal*: a distortion with a
blendable analog octave-up and a separate booster. It is a behavioural model built from public
descriptions of the pedal, **not** a schematic-exact copy, and is not affiliated with either company.

## Signal flow
```
in -> [Octave-up blend] -> [RAT-style distortion: gain, GBW + slew limit, diode clipper, Filter, Amplitude] -> [Boost "Magnitude"] -> out
```
Boost position can be switched to *Pre-distortion* (assumption: the real pedal's exact order is not documented here).

## Controls
| Control | What it does |
|---|---|
| Magnitude | Clean MOSFET-style boost, 0 to +28 dB (soft saturation) |
| Octave | Blends a full-wave-rectified octave-up into the signal (mono, best on single notes high on the neck) |
| Distortion | Op-amp gain (about 22x to 2100x), bandwidth falls as gain rises like a LM308 |
| Filter | Low-pass after the clipper, about 32 kHz (left) to 475 Hz (right) |
| Amplitude | Distortion level, centre is about unity |
| Clipping | Silicon (stock), Germanium (softer, lower threshold), LED (harder, louder) |
| Boost / Distortion | The two footswitches (click-free crossfade) |
| Output | Host-side trim, -24 to +12 dB |

DSP runs at 4x oversampling (reports 4x-oversampler latency to the host).

## Build
Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.15 is downloaded automatically.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Result: `build/LifePedalSim_artefacts/Release/VST3/Life Pedal Sim.vst3`

Install:
- Windows: copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (unsigned builds may need `xattr -cr` on the bundle)
- Linux: `~/.vst3/`

No compiler? Push this folder to a GitHub repository: the included workflow
(`.github/workflows/build.yml`) builds Windows, macOS and Linux VST3s and attaches them as downloadable artifacts.

Optional offline DSP test: `cmake -S . -B build -DLIFEPEDAL_BUILD_TESTS=ON && cmake --build build --target dsp_test && ./build/dsp_test`

JUCE is used under its AGPLv3/commercial licensing: if you distribute the plugin, the AGPL terms apply.
