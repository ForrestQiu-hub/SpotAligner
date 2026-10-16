# Spot Aligner

A spot microphone aligner and placer for classical recording, as a VST3 plug-in.
Free software, GPLv3.

> **It is a stereo effect.** It carries one mono object — a spot microphone — and
> places it in the stereo image, so put it on a **stereo track**. (The two input
> channels are summed into that one object.) On a mono track nothing is
> processed: the object passes through and the panel says so.
>
> **Do this on the spot track only.**

## Download

**[SpotAligner-1.0.0-win64.zip](https://github.com/ForrestQiu-hub/SpotAligner/releases/latest/download/SpotAligner-1.0.0-win64.zip)** — Windows 10/11, 64 bit, VST3.

Or see [all releases](https://github.com/ForrestQiu-hub/SpotAligner/releases). Unzip, run `Install.bat`, and let your host scan for plug-ins. A README is
included in English, Chinese, German, French and Japanese.

## What it does

A spot microphone is not a pan knob. What a spot *should* sound like is what it
would have been in the main array: delayed by the time the sound takes to reach
each microphone, and at the level each of them hears. Spot Aligner computes
exactly that from the geometry you describe.

* **Alignment delay** from the distance and the speed of sound — entered by hand
  or computed from temperature, humidity and air pressure.
* **Six stereo techniques** drive the per-channel delay and level: AB, ORTF, XY,
  NOS, DIN and Decca Tree. A technique is a **preset**: its spacing, aiming and
  polar pattern are loaded and stay editable afterwards, and the panel marks the
  array `edited` once it no longer matches its technique.
* **A Decca Tree really has its centre microphone** in the audio path, with its
  own distance to the source, fed to both sides.
* **Readouts**: the recording angle of the array, the delay and the level of each
  side, and where the object sits in the image.
* The model is the one from the published tables: `sin(SRA/2) = speed · 1.5 ms ÷
  spacing` reproduces them, and the 18 dB / 1.5 ms constants of equivalence
  stereophony reproduce the Blumlein angle of 76° to within 0.3° — derived from
  "coincident, ±45°, figure of eight" alone.

## Why

Because the usual way of adding a spot — a level panner, or a hand-set delay
guessed by ear — throws away the thing that makes it sit in the room. The
physics here is not an effect; it is bookkeeping, done properly and shown to you.

## Requirements

Windows 10 or 11, 64 bit, and any VST3 host (REAPER, Nuendo, Cubase, Studio One,
Bitwig, WaveLab …). A **stereo track** is required, as described above.

**Pro Tools** only loads AAX, and refuses unsigned AAX, so use a wrapper host
(Blue Cat PatchWork, DDMF Metaplugin, or the free Modalics Plugin Buddy) to run
this VST3 inside it. A macOS / AU build is planned; the AU wrappers ship with the
VST 3 SDK and need no code signing.

## Building from source

Needs CMake 3.14+, Visual Studio 2022 or newer, and the
[VST 3 SDK](https://github.com/steinbergmedia/vst3sdk) 3.8.x.

```sh
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSMTG_VST3_SDK_ROOT=<path to vst3sdk>
cmake --build build --config Release --target SpotAligner
```

The SDK can also be put in a `vst3sdk` folder next to this project, or named by
the `SMTG_VST3_SDK_ROOT` environment variable.

The physics lives in four dependency-free headers — `spotPanner.h`,
`formatRules.h`, `panoramaGeometry.h`, `soundSpeed.h` — and every rule in them is
covered by headless tests that need neither a host nor a GUI.

## Not done yet

The off-axis colouration of the spot microphone itself is **not** applied: that
microphone's polar pattern is already in its recording, and applying it again
would count it twice. It is a note in the panel, not a filter.

## Status and history

`ITERATION-REPORT.txt` records the whole development in order: what each step
did, why, and the root cause and fix of every mistake — 29 parameters, 47
validator tests, contract and layout checks all passing.

## Feedback

Questions, corrections and suggestions: **qiuzhenyan@163.com**

## Credits

**Zhenyan Qiu**, School of Physics, Nankai University.

Built with the VST 3 SDK. VST is a trademark of Steinberg Media Technologies
GmbH.

## Licence

GPLv3 — see [LICENSE](LICENSE).
