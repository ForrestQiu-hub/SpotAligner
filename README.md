# Spot Aligner

A spot microphone **aligner and placer** for classical recording. Windows x64 VST3, free software (GPLv3).

**Download:** [SpotAligner-1.0.0-win64.zip](https://github.com/ForrestQiu-hub/SpotAligner/releases/latest/download/SpotAligner-1.0.0-win64.zip) · [all releases](https://github.com/ForrestQiu-hub/SpotAligner/releases) · **Feedback:** qiuzhenyan4135@163.com
**中文说明:** [README.zh-CN.md](README.zh-CN.md) · The package also carries the README in English, Chinese, German, French and Japanese.

---

## Introduction

In classical recording the main pair carries the whole picture while spot microphones add definition and presence. When a spot is brought in there are usually only two knobs: level and pan. But **a spot microphone is not a pan knob** - it already has a definite place in the room: a distance from the main array and an angle.

Spot Aligner works that place out and applies it to the signal:

* an **alignment delay** from the distance to each main microphone and the speed of sound, so the spot lines up with the main pair in time;
* from the **technique of the array** (AB / ORTF / XY / NOS / DIN / Decca Tree), the delay and the level each side should receive, so the spot lands where it really was in the room.

The panel shows the recording angle of the array, both delays and both levels, and marks the array as `edited` once it no longer matches the geometry of its technique. It is free because the thing is worth doing and too small a niche for anyone to build a company around.

## Principle

**1. Why the alignment comes first.** The spot is close to the instrument (0.5-3 m) and the main pair is far away (5-15 m), so the spot arrives early. Once two copies of a sound are more than about 1 ms apart, the direction is decided by whichever arrived first, and the spot pulls the image to itself. The alignment delay puts the spot level with the main pair, lets the two fuse again, and hands the decision back to the main pair.

**2. Why the two sides get different delays and levels.** The technique decides how the pair images: AB and Decca Tree by **time difference**, XY by **level difference**, ORTF / NOS / DIN by both. A spot that is to sit in the same image has to carry the same cues, so the plug-in works out both from the geometry instead of leaving the user with a knob that has nothing to do with the room.

**3. A Decca Tree really has its centre microphone.** It is not "a wide AB": the centre microphone has its own delay line, is fed equally to both sides, and therefore dilutes the outer pair's cues - the same spacing covers a wider angle once it is there.

**4. Two boundaries, stated honestly.** Close to the source the instrument cannot be treated as a point, so a geometry-derived alignment is a representative approximation; what is exact is the main array's side, which is far away. And any left/right time difference dt gives this track comb nulls in mono at **f = 1/(2 dt)** - the inherent price of time-difference imaging, which an AB main pair pays as well.

**5. The spot's own polar pattern is not applied again.** That microphone's pattern is already in its recording; applying it a second time would count it twice. It is a note in the panel, not a filter.

The model is not invented here: the equivalence of time and level differences, the 18 dB / 1.5 ms constants and the recording-angle calculation come from the German recording school. The full literature review with its evidence grading is kept in the repository.

## Requirements

* **Windows** 10 or 11, 64 bit; any VST3 host (REAPER, Nuendo, Cubase, Studio One, Bitwig, WaveLab).
* **IT MUST GO ON A STEREO TRACK.** The plug-in carries one mono object - a spot microphone - and places it in the stereo image, so both channels are used: the two input channels are summed into that one object, which is then distributed by the geometry. When the track cannot carry two channels nothing is processed (the object passes through) and the panel says so; `Track output: 2 channels` means the track is right.
* **Pro Tools** loads AAX only and refuses unsigned AAX, so use a wrapper host (Blue Cat PatchWork, DDMF Metaplugin, or the free Modalics Plugin Buddy) to run this VST3 inside it. A macOS / AU build is planned - the AU wrappers ship with the VST 3 SDK and need no code signing.
* The files are not code signed, so Windows may ask once when the plug-in is first scanned: choose "More information" and "Run anyway".

## How to use it

**Install:** run `Install.bat` (it picks the right folder for the rights it has), or copy the `SpotAligner.vst3` folder into

```
C:\Program Files\Common Files\VST3\        all users, administrator
%LOCALAPPDATA%\Programs\Common\VST3\       this user only
```

Then let the host scan for plug-ins. It appears as **Spot Aligner** by **Forrest's Plug-ins**.

1. Put it on the **spot track only**, never on the main pair.
2. Enter **Distance / Angle / Height** - where the source sits relative to the main array.
3. Choose **Format**; the plug-in loads that technique's spacing, aiming and polar pattern. All three stay editable afterwards, and the panel marks the array `edited` once they leave the standard values.
4. **Leave the host's pan centred.** The plug-in has already placed the spot by geometry; panning it again places it twice.
5. **Check in mono.** With a left/right time difference there are comb nulls at f = 1/(2 dt); that is inherent to time-difference imaging, and it is a good way to judge whether the spot is set too loud.

**The L/R Time Difference switch.** On (the default) the two channels are delayed to their own microphone, so the object is placed by the geometry. Off, both channels share **one** delay - that of the main microphone on the side the object is nearer, and the left one when it sits on the centre line: the traditional way, left to left, right to right, the centre line to the left. Because AB and Decca Tree image by time difference while their physical level difference is usually under 1 dB, switching it off converts the placement the time difference carried into a level difference at the model's own rate (1.5 ms to 18 dB) - 0.92 ms becomes about 11 dB - so the object still lands at the angle the geometry gives, and the pair's own time difference is left untouched.

Note that with it off, **Angle is still a geometry input, not a free pan control**: changing it also moves the distance, the off-axis angle and the reference microphone, and the level difference follows the array's own imaging law (roughly the sine of the angle for a spaced pair, the polar patterns for a coincident one) rather than a host pan law. Keep the host's pan centred.

**When the spot is itself a pair** (a piano AB pair, for instance):

* one track and one plug-in instance per microphone, with **identical settings**, and the point (the angle) taken at the **midpoint** of the two;
* the plug-in then only shifts a source that has width to the position of that midpoint, and the pair's own time difference is not touched at all - the width comes from the pair, the position from the plug-in;
* both tracks must let the plug-in see a stereo bus, otherwise it treats them as mono and passes through.

**Uninstall:** delete the `SpotAligner.vst3` folder. Settings are stored with the project, and older projects keep working, as do state versions from the first release onwards.

---

Built with the VST 3 SDK 3.8.1. VST is a trademark of Steinberg Media Technologies GmbH.
From source: CMake 3.14+ and Visual Studio 2022 or newer; the physics lives in four dependency-free headers (`spotPanner.h`, `formatRules.h`, `panoramaGeometry.h`, `soundSpeed.h`) and every rule in them is covered by headless tests that need neither a host nor a GUI.

**Author:** Zhenyan Qiu, School of Physics, Nankai University · **Licence:** [GPLv3](LICENSE) · Development history: [ITERATION-REPORT.txt](ITERATION-REPORT.txt)
