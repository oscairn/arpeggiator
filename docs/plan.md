# Oscairn Arpeggiator: plan (coverage and port)

This document tracks coverage against the reference and the port to Rust. The design plan, with the parameters, the
playback rules, the milestones and the known differences, is [doc/plan.md](../doc/plan.md); the phrase format is
[doc/phrase-format.md](../doc/phrase-format.md).

## 1. Status

Version 0.1.8. Which build of the arpeggiator the 2026-11-17 conversion run used is not recorded in the inputs:
unverified. Implementation language today: C++. Everything in Oscairn's plugin set is to end up in Rust: the engines port after their coverage work, and new effect types are written in Rust from the start.

## 2. Supported

The reference's arpeggio parameters map onto these plugin parameters, with the Performance-level ones resolved by
the converter or not carried yet ([doc/plan.md](../doc/plan.md), section 1.1): Switch, Hold (Sync-Off, Off, On), Key Mode, Velocity Mode, Trigger,
Loop, Play Only, Unit Multiply, Quantize Value and Strength, Swing, Velocity Rate, Gate Rate, Octave Range, Output
Octave, note and velocity limits, Change Timing, Accent Threshold and Quantize, Random SFX with its velocity settings,
Fixed SD/BD, Sync Quantize, Slot, per-slot velocity and gate offsets, and Chord Source. Phrases play from eight slots
in the plugin state; how each playback rule compares with the reference is in [doc/plan.md](../doc/plan.md),
sections 1.2, 9 and 10.

| Area | Played today | Not yet |
|---|---|---|
| Phrases | chord-relative, walker, template and absolute notes; controller and pitch-bend lanes; patterns per held-key count | - |
| Style tracks | root-fixed voices on the measured rule | root-transposed voices and other transposition tables (approximated) |
| Performance level | - | the Performance-level arpeggio switch (folded into each part's switch by the converter); scene arpeggio select |

## 3. Coverage

Measured on 2026-11-17. Two sets of Performances of the reference were converted: its factory (stock) set of 3631 Performances and a personal library of 87 Performances. Neither set has a Performance with no sound-affecting drop yet (0 of 3631, 0 of 87): every Performance still needs work somewhere, in this plugin or elsewhere. There is no per-part coverage report for the arpeggiator; what it covers shows as
the arpeggio gaps below.

Parts converted with the arpeggiator in front of their engine, stock set (every stored arpeggio kept): 240 on the VA
engine, 3205 on the Sampler, 305 on the FM engine. A further 697 drum-kit parts and 322 sampled parts with an
arpeggio are muted (the drum kits, and sampled parts that could not be packed on the measuring machine or have no
element that can play). Personal library (an arpeggio kept only where it is switched on):
10 on the VA engine, 8 on the Sampler.

### Gaps

11 gaps of the stock set involve this plugin; each is shared with the converter. "Changes the sound" is a classification of the gap, not a listening test: a stored value away from its default that is not carried counts as sound-affecting whether or not it is heard in a given Performance. Counts are Performances and parts of the stock set that carry the gap; parts switched off or muted are not counted against their other gaps.

| Gap | Performances (stock) | Parts (stock) | Changes the sound | Status |
|---|---|---|---|---|
| Arpeggio Master Switch (approximated: off: folded into the part's arpeggio Switch (no Performance-level switch yet)) | 1525 | 1898 | yes | open |
| Arp 1-8 Number (approximated: style track: transposition approximated) | 575 | 855 | yes | open |
| Arp Fixed SD/BD (dropped: acts only on Drum Parts (measured); off here) | 8 | 9 | yes | open |
| Arp 1-8 Number (dropped: event BN skipped) | 8 | 9 | yes | open |
| Arp 1-8 Number (dropped: negative gate played as one tick) | 2 | 2 | yes | open |
| Scene Arpeggio Select (dropped: not converted yet) | 2 | - | yes | open |
| Arp Quantize Strength (approximated: with the Common offset it sums to N, beyond N..N; limited) | 1 | 4 | yes | open |
| Arp Velocity Rate (approximated: with the Common offset it sums to N, beyond N..N; limited) | 1 | 3 | yes | open |
| Swing (approximated: with the Common Swing Offset it sums to N, beyond N..N; limited) | 1 | 2 | yes | open |
| Arp Gate Time Rate (approximated: with the Common offset it sums to N, beyond N..N; limited) | 1 | 1 | yes | open |
| Arpeggio Velocity Rate Offset (dropped: not converted yet) | 1 | - | yes | open |

Only in the personal library (not in the stock set):

| Gap | Performances (personal) | Parts (personal) | Changes the sound | Status |
|---|---|---|---|---|
| Arpeggio Gate Time Rate Offset (dropped: not converted yet) | 3 | - | yes | open |

## 4. Port to Rust

Status: not started.

A port counts as done when all of these hold: the test suite and the cost gates below are carried over and pass against the Rust build; a saved state of the C++ version opens and plays the same; the plugin id, the parameter ids and the sound format stay the same. For this plugin:

- Tests: the suite in `tests/` (phrase, player, plugin and record tests, the privacy check).
- Cost gate: the repository has none today.
- Saved state: the state format in [doc/phrase-format.md](../doc/phrase-format.md) opens, with its eight slots.
- Same plugin id, parameters and phrase format (`oscairn-phrase 1`).

## 5. Next

1. A Performance-level arpeggio switch (1525 stock Performances approximated today), with the converter.
2. Style tracks: the transposition of root-transposed voices and the other tables (575 Performances approximated).
3. The rest of the arpeggio settings the converter does not carry (seventeenth and last cross-engine package; 20
   stock Performances), then the open milestones of [doc/plan.md](../doc/plan.md), section 7.
