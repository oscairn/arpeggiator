# Oscairn Arpeggiator

A phrase-based arpeggiator for [Oscairn](https://github.com/oscairn/oscairn), as a CLAP note effect
for macOS and Linux. It plays *phrases*: looped patterns whose notes refer to the keys you hold, by
position in the chord, as walkers up and down the chord, as template notes that follow the chord, or
as absolute keys, with controller and pitch-bend lanes alongside. A phrase can hold different
patterns for one, two, three or more held keys.

It aims to play everything a MONTAGE M / MODX M arpeggio does, so that arps converted from those
performances (by [performance-converter](https://github.com/oscairn/performance-converter), on your
own machine from your own installation) play the same. This repository contains no instrument data
or phrases; it reads the open [phrase format](doc/phrase-format.md), which is also meant for writing
your own.

Status: early. See the [plan](doc/plan.md) for what is implemented, what is measured against the
reference and what is not.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

This builds `build/oscairn-arpeggiator.clap`. The CLAP headers are vendored in `third_party/clap`
(MIT; commit in `third_party/clap/upstream.json`).

## Use

- Put the arpeggiator in front of an instrument in a part's chain. Phrases live in eight slots in
  the plugin state; the **Slot** parameter selects one. Load a phrase file through the host's
  preset loading (it goes to the selected slot), restore a saved state, or push one in on the note
  input ([format](doc/phrase-format.md#pushing-a-phrase-at-run-time)).
- `tools/midi_to_phrase.py clip.mid out.ophr --mode chord --chord 60,64,67` turns a MIDI clip into
  a chord-relative phrase (`--mode fixed` or `relative` for absolute or transposing phrases).
- `build/arp-render PHRASE [--set name=value]... on@0:60:100 off@2:60` prints what a phrase plays,
  offline and deterministically.

## Parameters

Switch, Hold (Sync-Off, Off, On), Key Mode (Sort, Thru, Direct, Sort+Direct, Thru+Direct),
Velocity Mode, Trigger (Gate, Toggle), Loop, Play Only, Unit Multiply, Quantize Value and Strength,
Swing, Velocity Rate, Gate Rate, Octave Range, Output Octave, Note and Velocity limits (a low limit
above the high one selects the outer ranges), Change Timing, Accent Threshold and Quantize,
Random SFX with its velocity settings, Fixed SD/BD, Sync Quantize, Slot, and per-slot velocity and
gate offsets.

## Licence

MIT. See [LICENSE](LICENSE).
