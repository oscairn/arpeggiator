# Oscairn Arpeggiator: plan

Status: 2026-11-01. Owner: Jacob Schooley.

A phrase-based arpeggiator for [Oscairn](https://github.com/oscairn/oscairn): a CLAP note effect
that turns held keys into notes and controllers by playing a *phrase*, a short looped pattern whose
notes refer to the held chord. The target is everything a MONTAGE M / MODX M arpeggio does, so that
the performance converter can carry those arps over, and a phrase engine general enough to drive a
motion-sequencer-style lane later. MIT licence, built from scratch: no open-source arpeggiator plays
chord-relative phrases.

The repository holds no instrument code, data, phrases or presets. Phrases come from an open,
documented text format ([phrase-format.md](phrase-format.md)); converted factory and user phrases
are produced on the user's machine from their own installation and are never committed.

## 1. Target

### 1.1 Parameters

Every arpeggio parameter of a MONTAGE M / MODX M Part and Performance, from the published Data
List (MIDI parameter tables, Part `1p 00 06 00` arp block, Part `1p 00 01 00` switches, Part
`1p 00 02 00`/`03 00` swing and rates, Common `06 00 06 00` arp block, scenes) and the Operation
Manual's arpeggio pages. The plugin column is the CLAP parameter (section 4.3).

| Reference parameter | Range | Plugin | Notes |
| --- | --- | --- | --- |
| Part Arp Switch | Off, On | Switch | |
| Arpeggio Master Switch (Common, Scene) | Off, On | — | Performance level: a Performance control on every arp's Switch |
| Arp Hold | Sync-Off, Off, On | Hold | Sync-Off: runs silently while released, resumes in place |
| Arp Key Mode | Sort, Thru, Direct, Sort+Drct, Thru+Drct | Key Mode | |
| Arp Velocity Mode | Original, Thru | Velocity Mode | |
| Arp Trigger Mode | Gate, Toggle | Trigger | |
| Arp Loop | Off, On | Loop | Off: one pass per key-on gesture |
| Arp Play Only | Off, On | Play Only | Played keys never sound themselves |
| Arp Unit Multiply | 50 … 400 %, Common | Unit Multiply | Common is resolved by the converter |
| Unit Multiply (Common, Scene) | 50 … 400 % | — | Converter |
| Arp Quantize Value | 60 … 480 clocks | Quantize Value | 480 per quarter |
| Arp Quantize Strength | 0 … 100 % | Quantize Strength | |
| Swing (Part; Common offset; Scene) | −120 … +120 | Swing | Offsets summed by the converter |
| Arp Velocity Rate | 0 … 200 % | Velocity Rate | |
| Arp Gate Time Rate | 0 … 200 % | Gate Rate | |
| Arp 1–8 Velocity / Gate Time Rate Offset | −100 … +100 % | Slot n Velocity / Gate Offset | |
| Arpeggio Velocity / Gate Time Rate Offset (Common, Scene) | −100 … +100 % | — | Converter adds to the part rates, or a Performance control |
| Arp Octave Range (+ Common offset) | −3 … +3 | Octave Range | |
| Arp Output Octave Shift (+ Common offset) | −10 … +10 | Output Octave | |
| Arp Note Limit Low/High | C−2 … G8 | Note Low / High | Low above High: the two outer ranges |
| Arp Velocity Limit Low/High | 1 … 127 | Velocity Low / High | Same inversion rule |
| Arp Change Timing | Real-time, Measure | Change Timing | When a new slot takes over |
| Arp Accent Velocity Threshold | Off, 1 … 127 | Accent Threshold | |
| Arp Accent Start Quantize | Off, On | Accent Quantize | |
| Arp Random SFX | Off, On | Random SFX | |
| Arp Random SFX Velocity Offset | −64 … +63 | SFX Velocity Offset | |
| Arp Random SFX Key On Control | Off, On | SFX Key Velocity | |
| Arp Fixed SD/BD | Off, On | Fixed SD/BD | Drum parts |
| Arp 1–8 Number (+ Extra) | preset, user, library | 8 phrase slots in the state | |
| Arpeggio Select (Common, Scene) | 1–8 | Slot | |
| Arpeggio Synchro Quantize Value | Off, 60 … 480 | Sync Quantize | |
| Arpeggio Group (Part) | Off, A … P | — | Parts sharing settings: Performance controls targeting each part's arp |
| Arpeggio Quantize Strength Offset (Common) | −100 … +100 | — | Converter |
| Scene arp memorize switches | | — | Oscairn scenes store parameter values |

### 1.2 Playback rules

Each rule has a source: **Manual** (Operation Manual text), **Measured** (confirmed by recording the
reference, section 6) or **To measure** (our working rule until a recording confirms or corrects it). Nothing is "done" until it is
Measured or explicitly accepted by Jacob.

| # | Rule | Source |
| --- | --- | --- |
| R1 | A phrase has up to 16 tracks that play together; each track sounds only while the number of held keys is within its own minimum and maximum. Factory phrases use this to give 1, 2, 3 and 4+ held keys different patterns | Measured for 3 held keys (`rnb3`); keys and timing for 1, 2, 3 held (`pf*_hi`) with the exceptions in section 9 |
| R2 | A track's note list is its template notes (each either following the chord or absolute) followed by the held keys, optionally without the first held key; or only the held keys | Measured (template + held: `rnb3`) |
| R3 | Held keys are listed by pitch (Sort) or in the order played (Thru); Direct passes the played keys through; Direct alone plays only the phrase's controllers | Sort and Thru Measured; Direct to measure |
| R4 | Template notes that follow the chord move with the lowest held key and snap their 2nd, 3rd/4th, 5th and 6th/7th degrees to the held chord's intervals | Measured on a major triad (`rnb3`); other chords to measure |
| R5 | Twelve selection rules: by position (none, last or first when the list is shorter), bouncing and cycling positions, walkers up, down, up-down (ends once or twice) and random over 1–4 octaves, and style transposition of literal pitches | Measured: index, walker up (1 and 2 octaves), up-down (ends once). Random: timing only (the reference's random sequence is not reproduced, KD13). Others to measure |
| R6 | Position notes carry an octave shift of −4 … +3 | Measured (−1: `pf*_hi`) |
| R7 | Octave Range moves the whole phrase one octave further on each repetition until the range is reached, then starts over; for walkers it widens the walked octaves | Walker Measured (+1, `oct_walk`); index phrase: matched notes agree in key and timing, short notes the analysis cannot separate remain (`oct_idx_hi`) |
| R8 | Output Octave Shift moves every chord-relative note; absolute template notes stay | Measured for chord notes (−1, `outoct_hi`); absolute notes to measure |
| R9 | Velocity: the phrase's (Original) or the played key's (Thru), times Velocity Rate plus the slot offset; 0 becomes 1, above 127 becomes 127 | Measured: Original, Thru, rate 50 % (non-overlapping notes, KD15) |
| R10 | Gate: the phrase's gate times Gate Rate plus the slot offset; 0 becomes 1 | Measured: 50 % and 200 % (gate times) |
| R11 | Unit Multiply scales event times and gates by 50, 66.7, 75, 100, 133.3, 150, 200, 266.7, 300 or 400 % | Measured: 66 % and 200 % |
| R12 | Quantize moves events toward the Quantize Value grid by Strength; Swing delays the grid's even-numbered positions, with or without quantize strength | Swing timing Measured (+40, with strength 0 and 100); strength between 0 and 100 to measure |
| R13 | Hold Off stops at the last release, ending sounding notes; On keeps the chord until the next gesture replaces it; Sync-Off ends sounding notes at the release but keeps the phrase position running silently and resumes in place | Measured (`uprel`, `holdon`, `syncoff`) |
| R14 | Trigger Toggle: a gesture (keys pressed together) starts the arp, the next stops it | Measured (`toggle`) |
| R15 | Loop Off: one pass per gesture | Measured (`loopoff`) |
| R16 | Note and velocity limits decide which keys feed the arp; with the arp on, keys outside the note limits do not sound, keys outside the velocity limits play as they are; Low above High selects the outer ranges | Measured (both, normal ranges); inverted ranges to measure |
| R17 | A played velocity at or above the Accent threshold adds the phrase's accent tracks, on top of the normal ones, for one pass, at the quantize grid if Accent Start Quantize is on | Measured: accent tracks add, below the threshold nothing changes (`house_acc`, `house_noacc`); start timing to measure |
| R18 | Random SFX: a key release plays a random note from the phrase's SFX ranges at a random velocity in its range (or the key's velocity, with Key On Control), plus the velocity offset | Manual; to measure |
| R19 | Fixed SD/BD maps the phrase's kick and snare variants onto the kit's main kick (36) and snare (38) | To measure |
| R20 | Phrase controllers (CC, pitch bend, pressure) play on their ticks while the track's held-count range is met and, for tracks with a chord filter, the held chord passes it | To measure |
| R21 | Change Timing: a new slot starts immediately or at the next bar | Manual; to measure |
| R22 | Sync Quantize: a starting arp waits for the next grid line so parts started together stay together | Manual; to measure |
| R23 | Per-track randomize: notes in a key/velocity window get a random key and velocity offset | To measure |
| R24 | Walker positions continue across loops; they restart with the arp | To measure |
| R25 | Style tracks transpose literal pitches written for a source chord to the held chord, with per-range rules (root transposition or fixed, transposition table, bass, range folding) and a chord filter | Approximate first; to measure |

R25 (about 30 % of factory tracks, mostly "_C"/genre arps) needs chord recognition and the note
transposition tables; it lands as an approximation (milestone M4) and is measured separately.

## 2. Phrase engine

A **phrase** (doc/phrase-format.md) has a resolution (PPQ), a loop length in ticks, a meter, optional
SFX ranges, and up to 16 **tracks** of timed **events**:

- notes: a pitch source (absolute `k60`, relative to the lowest held key `r60`, note-list position
  with octave `p2+1`, position with an absolute fallback `p2/k64`), velocity and gate in ticks;
- controllers, pitch bend and channel pressure.

Each track has a selection rule, a note-list recipe (held keys, template notes), a held-count range,
an accent flag, walker octaves, randomize windows, a chord filter and, for style tracks, transposition
rules. Playback rules from section 1.2 apply on top, driven by the plugin parameters.

The same engine can later play motion-sequencer lanes: a lane is a track of controller events
(or a future `value` event type aimed at a parameter through Oscairn's param-target lanes, RFC 0002),
and the clock, unit multiply, swing, key-on reset and loop logic are shared. That extension is not in
this plan's milestones.

**Real time:** the phrase and the player use fixed-size storage (4096 events per phrase, 16 tracks,
32 held keys, 256 sounding notes). Parsing allocates nothing, so a phrase can be parsed into a spare
slot and swapped. The player never allocates, locks or does I/O; randomness is a seeded generator in
the state, so offline renders are deterministic.

## 3. Phrase format and where phrases come from

[phrase-format.md](phrase-format.md) is the open, versioned (`oscairn-phrase 1`) text format, meant
to be written by hand as well as by tools. Sources:

1. **Converted from the instrument:** `performance-converter` turns factory and user arpeggios into
   phrase files on the user's machine (section 5). Never committed or published.
2. **Imported MIDI:** `tools/midi_to_phrase.py` turns a MIDI clip into a phrase: absolute keys,
   keys relative to the lowest held key, or chord-relative positions against a stated chord (like the
   reference's user-arp recording with its Fixed / Normal / Org-Note conversions).
3. **Recorded:** a record mode in the plugin (milestone M5) captures what is played for a set number
   of bars against the held chord and stores it as a phrase in the selected slot, chord-relative or
   absolute.
4. **Edited:** a phrase editor (below).
5. **Generated at run time:** pushed by the host or another plugin (below).

**Phrase editor data model.** The editor works on the same model as the file: phrase header, tracks
(each with its rule settings), and per-track event lists. A grid view per track shows time against
pitch source: rows are note-list positions (with octave lanes) for chord-relative tracks and keys for
absolute ones; each cell holds velocity and gate. Controllers are lanes of points. Operations: add,
move, delete, set velocity/gate, quantize, set track rule and held-count range, duplicate a track for
another held count, and audition with a test chord. Oscairn's editor system (RFC 0011 structured
editors, RFC 0019 editor descriptions) would need: a **step/pattern grid** rich widget bound to plugin
operations (not parameters), operations with arguments (`phrase.set-event`, `phrase.insert`,
`phrase.delete`, `track.set`), a way for the plugin to publish the phrase model to the editor (a
read operation returning the text form is enough), and undo through the host. No UI is built in
Oscairn by this project.

**User phrases in the library.** A phrase file is a small text asset. A Sound (RFC 0021) stores the
plugin state, which embeds every slot's phrase text, so a saved Sound or Performance is
self-contained and moves between machines with nothing else. Separately, phrases the user makes or
imports are library assets (RFC 0009, the user library: "phrase and arpeggio players: MIDI files,
pattern libraries") so they can be browsed, shared and exported as a library; when Oscairn's
`oscairn.assets` extension exists the state also records each slot's asset reference, and the embedded
copy remains the fallback. Converted factory phrases are a library that is exported as a reference
only (RFC 0009's rule for content that may not be redistributed).

**Phrases from outside.** The plugin depends on no scripting. It accepts a phrase:
- from its **state** (CLAP state extension; the state format is documented in phrase-format.md);
- from a **file** chosen in the host (main thread; parsed, then swapped in);
- **pushed** in-band on its note input as a CLAP `MIDI_SYSEX` event carrying an `oscairn-phrase`
  message (documented in phrase-format.md): slot number plus the phrase text. A future MIDI script
  block, a sequencer or the host can generate phrases and send them; the audio thread copies the
  text into a preallocated inbox, the main thread parses it, and the phrase takes over its slot at
  the next block (a main-thread round trip, not sample-accurate).

## 4. Oscairn integration

### 4.1 Plugin

A CLAP note effect (`org.oscairn.arpeggiator`): one note input, one note output, no audio ports,
CLAP and MIDI dialects. Output notes carry their own note IDs; played keys passed through keep a
mapping from input to output identity and release with their input. `NOTE_END` is sent when a note
the plugin owns is released. Host chokes (`NOTE_CHOKE`) end the matching outputs. Pitch bend and
other channel messages pass through. Sustain (CC 64) latches the arp's keys while down (like Hold) and
is passed through only for keys the arp does not own. Builds for macOS and Linux ARM64 (and x86_64
for CI).

### 4.2 Clock and transport

The arp clock runs at the host tempo (`clap_event_transport.tempo`, 120 if absent), so it plays with
the transport stopped, as the reference does. Sync Quantize aligns starts to the host beat timeline
while the transport plays (RFC 0012: Oscairn's transport, internal or following MIDI clock or
Link), so every part's arpeggiator lines up. A playing-to-stopped transport transition ends all
sounding notes. Unit Multiply scales the arp clock against the tempo.

**While the transport is stopped** there is no shared beat grid yet (RFC 0032 step 3 adds a
free-running host beat). Until then Sync Quantize waits for the next grid line of the instance's
own arp clock, which starts when the instance activates: starts quantize correctly within one part,
but two parts are not guaranteed to land on the same grid line (Known differences, KD6). When the
host's free-running beat exists, the plugin reads it exactly as it reads the playing transport.

### 4.3 One instance per Part

Each converted Part gets one arpeggiator instance at the front of its chain (RFC 0021: a Sound is a
saved part chain, "an arpeggiator in front of the synth"). Performance-level settings (master switch,
arp select, common offsets, Arpeggio Group) become Performance controls that target each part's
parameters (RFC 0011 / RFC 0002), or are folded into the part parameters by the converter when they
are static. Parameters (stable IDs):

Switch, Hold, Key Mode, Velocity Mode, Trigger, Loop, Play Only, Unit Multiply, Quantize Value,
Quantize Strength, Swing, Velocity Rate, Gate Rate, Octave Range, Output Octave, Note Low, Note High,
Velocity Low, Velocity High, Change Timing, Accent Threshold, Accent Quantize, Random SFX, SFX
Velocity Offset, SFX Key Velocity, Fixed SD/BD, Sync Quantize, Slot, Slot 1–8 Velocity Offset,
Slot 1–8 Gate Offset, Record, Record Bars, Record Convert. Phrase slots live in the state.

No pitch limit applies beyond the documented parameter ranges: walkers span their octaves plus
the full Octave Range, and only the MIDI key range (0–127, folded by octaves) bounds the result.

### 4.4 What the converter needs

`performance-converter` (MIT, published) maps each Part's arp block to these parameters (value
tables in phrase-format.md, "Converter notes") and each `Arp n Number` to a phrase file. Arp file
reading is plain format reading (chunked file, config fields, events) and belongs in the converter;
nothing needs the instrument plugin's code or tables at run time. The extractor used for this plan
lives outside the repository and is described to the converter's owner.

## 5. Converting the instrument's arpeggios

Factory arpeggios are plain files inside the instrument plugin's bundle
(`…/Contents/Resources/contents/arpeggio/NNNNNN-Arpeggio.arp`, 10,922 files, indexed by
`arpeggio.cfg`); user and library arps are in the user content folder
(`…/contents/current/arpeggio/`). Each file is a set of chunks: a header, one chunk per track
(a configuration block followed by MIDI-like events) and name/length/meter/SFX metadata. The
mapping to the phrase format is one to one except: style tracks' transposition tables are
approximated (R25), a key-count remapping field that no factory file uses is reported, and a rare
4-byte controller record (18 factory files) is reported and skipped.

Output goes to `~/Projects/Open Synth Platform/converted/arps/` (never committed): `factory/NNNNN.ophr`,
`user/<id>.ophr`, `index.json`, `report.json`.

## 6. Measuring against the reference

The reference is the instrument's software (the macOS plugin), driven by a recorder that renders a
performance file while playing timed notes and controllers (host tempo 120 BPM).

- **Probe sound:** an FM Part with one sine carrier, instant attack, full sustain and instant
  release, velocity sensitivity at maximum. Onsets, pitch, level and note length are read per key
  by demodulating at each key's frequency. Probe chords sit high enough that adjacent keys separate
  within a 20 ms window.
- **Calibration:** a velocity sweep (1–127) with the arp off gives the level-to-velocity curve.
- **Alignment:** recorder timing jitters by an I/O buffer, so each render is aligned on its own first
  arp note (or a reference click); all other times are relative.
- **Comparison:** the engine renders the same phrase file with the same settings and key events
  offline (`arp-render`); notes are matched in order.
- **Order of work:** the most used settings in Jacob's factory and user performances first (counted
  from the performance files: Key Sort, Velocity Original, Hold Off/On, Octave Range 0, Unit 100 %,
  no quantize; arps such as `1/16 Up Oct1`, `MA_Pf Arp1`, `MA_ChordRandom2`, `1/16 Up/Down B Oct1`,
  `MC_6/8 Ballad`), then each rule R1–R25.

**Proposed gate (for Jacob to confirm):** per probe, every note matched; keys exact; onset times
within 2 ms of the reference after alignment (recorder resolution about one I/O buffer, so
differences are judged relative to the first note of the same render); velocity within ±1 after
calibration; gate length within 5 ms. A probe passes only if every note passes.

## 7. Milestones

| # | Deliverable | Gate |
| --- | --- | --- |
| M0 | Plan, format, repo, CI | This document; CI green on Linux x86_64/ARM64, macOS, ASan/UBSan, TSan |
| M1 | Phrase parser/writer, MIDI import, player with R1–R16, R20–R24, plugin with state and push | Unit tests per rule with synthetic phrases; plugin test (state round trip, block-size independence, no allocation in process) |
| M2 | Extractor and converter notes | All factory and user arps convert and parse |
| M3 | Measurements of the most used settings and arps | Proposed gate on each probe |
| M4 | Style tracks (R25), accent (R17), SFX (R18), Fixed SD/BD (R19) measured | Proposed gate |
| M5 | Record mode (done, tested), MIDI import in the C++ library, phrase editor operations, asset references | Tests; editor description draft agreed with Oscairn |
| M6 | Motion-sequencer lanes on the same engine | Separate plan |

## 8. Alignment with Oscairn RFC 0032

RFC 0032 (Oscairn's side of the arpeggiator) was read at Oscairn commit 6396d9c. This plan agrees
with it except where noted:

- **Pushed phrases are parsed on the main thread** (resolves RFC 0032's open question and an
  earlier disagreement between this plan and the format document). The audio thread only copies the
  message into a preallocated 256 KiB inbox (bounded time, no allocation) and asks for a main-thread
  callback; the phrase takes over its slot at the start of the first block after the main thread
  has parsed it. Latency is one main-thread round trip, not sample-exact as RFC 0032's
  "at an exact sample" puts it (KD9). One push is in flight at a time; another arriving meanwhile
  is dropped (to be counted once the host has a place to report it).
- **The in-band push waits for RFC 0032 step 1** (SysEx between plugins). Until Oscairn's graph
  carries `CLAP_EVENT_MIDI_SYSEX`, the push works with hosts that deliver SysEx; it is tested with
  this repository's own test host (`tests/test_plugin.cpp`).
- **The `7D` message never leaves the plugin:** the arpeggiator consumes every SysEx it receives
  and emits none, so the push cannot reach a MIDI out through it. Filtering `7D` at hardware MIDI
  outs (for messages sent past the arpeggiator) is the host's side.
- **Stopped transport:** see 4.2; the plugin uses its own clock until the free-running beat exists.
  A song-position jump while playing starts a running phrase again, at the next Sync Quantize line
  when one is set (RFC 0032).
- **Recording by playing:** the plugin has its own record mode (Record, Record Bars, Record Convert;
  phrase-format.md), which works in any CLAP host and needs nothing from Oscairn. RFC 0032's host
  note capture with a **Make phrase** action would give retroactive capture; it should call the same
  conversion. Today the conversion exists twice (C++ in `src/record.cpp` for the plugin, Python in
  `tools/midi_to_phrase.py`); moving MIDI import into the C++ library as the `phrase.import-midi`
  operation RFC 0032 describes is milestone M5. Whether both recorders are kept is Jacob's call.
- **`author` and `tags`** are in the format before version 1 is frozen.
- **Style tracks (R25):** the transposition is an approximation built from chord-tone rules; the
  tables are to come from measurement (KD1).

## 9. Known differences from the reference

One entry per known difference: what, size and where, cause, deliberate or unresolved, how to
reproduce, where in the code. Entries are added as measurements find them and removed when fixed.

| # | What | Size and where | Cause | Status | Reproduce | Code |
| --- | --- | --- | --- | --- | --- | --- |
| KD1 | Style tracks (R25) transpose source pitches by root plus chord-tone rules, not the reference's transposition tables | Style tracks on chords other than their source chord: `MA_Soul 1 _C` matches on C (`soul_c2`) but differs on Dm7 (27 of 36 notes, 9 missing) and G7 (all 36: the reference moves the voices G C E G down to F B D F) | Tables unknown; to be measured | Unresolved | `arp-render` a converted style phrase (for example `MA_Soul 1 _C`) on Dm7 and compare with the reference render | `Player::playStyle` |
| KD2 | A chord change while style notes sound does not re-pitch or stop them (the `rule … RETRIGGER` field is read but unused) | Style tracks only | Not implemented | Unresolved | Change the chord mid-note on a style phrase | `Player::playStyle` |
| KD3 | Chord recognition (for chord filters and style tracks) uses the plugin's own interval table; unusual voicings may be named differently | Chord filters with partial masks, style tracks | Own design | Unresolved, to measure | Hold an unusual voicing on a style phrase | `Player::detectChord` |
| KD4 | Walkers have no octave ceiling: octaves plus the full Octave Range are walked | Walker tracks with many octaves and a large positive Octave Range; the reference may limit the span (to measure) | Jacob's rule: no limit beyond parameter ranges | Deliberate | `octaves 4` phrase with Octave Range +3 | `Player::playEvent`, walker branch |
| KD5 | A key-count remapping field in the instrument's arp files (unused by every factory file) has no equivalent | None in factory content | No data uses it | Deliberate | The extractor's report lists any file that sets it | extractor (outside this repo) |
| KD6 | Sync Quantize with the transport stopped aligns to the instance's own clock, not a shared grid | Parts started together on a stopped transport may differ by up to one grid step | No shared beat while stopped (RFC 0032 step 3) | Deliberate until the host adds the beat | Two instances activated at different times, transport stopped, Sync Quantize 480 | `Player::startPhrase` |
| KD9 | Pushed phrases take over a block after the main thread parses them, not at the event's sample | All pushes | Parsing must not run unbounded on the audio thread | Deliberate | `tests/test_plugin.cpp`, push case | `Plugin::sysex`, `Plugin::mainThread` |
| KD10 | SysEx other than the push message is dropped, not passed on | Any SysEx on the note input | Keeps private messages inside the plugin | Deliberate | Send any SysEx through | `Plugin::sysex` |
| KD11 | Literal-note and fallback events of user arps recorded on the instrument are not converted | User arps made with the instrument's recording (none in Jacob's set) | No sample files to verify their layout | Unresolved | Convert such a file; the report names it | extractor |
| KD12 | A 4-byte controller record in 18 factory arps is skipped | Those 18 phrases | Meaning unknown | Unresolved | Extractor report | extractor |
| KD13 | Random walkers and random phrases play a different random sequence | Every `random` track (`cr`, `cr_hi`, `q50`); timing matches, keys cannot | The plugin's own seeded generator, so renders repeat exactly | Deliberate | `cr_hi` probe | `Player::nextRandom` |
| KD14 | A note on the same key starting about 10 ticks after another ends sounds, in the reference, as one long quieter note | `MA_Pf Arp1` with one held key, at 2.57 s of `pf1_hi` (also `pf2_hi`, `pf3_hi`) | Unknown | Unresolved | `pf1_hi` probe | `Player::noteOn` (same-key handling) |
| KD15 | Notes that overlap other notes read 2–4 velocity steps away from the engine's, while isolated notes agree within ±1 | `swing`, `gate200`, chords in `bal3_hi`, `syn_hi`, `house_acc` | Unknown: the arp's velocity, or how the probe sound mixes voices | Unresolved | Those probes | `Player::sendNote` |
| KD16 | An accent started mid-phrase: the engine plays one normal-track note the reference leaves out | `house_late` at 2.03 s | Accent start rule not yet known | Unresolved | `house_late` probe | `Player::keyOn` (accent), `Player::advanceTo` |
| KD17 | Random SFX: in the New Country probe the reference played no SFX note at the releases where the engine plays one | `sfx_nc` | Unknown (conditions for SFX) | Unresolved | `sfx_nc` probe | `Player::playSfx` |

## 10. Measurement results

Recorded 2026-11-01 (batches `b3`–`b5` under `forks/measurements/arp/`, not in this repository) with
the probe described in section 6 and compared with `arp-render` on the converted phrases. Each
render is aligned per phrase start (median offset), since the recorder places key events only within
an I/O buffer. Velocity uses a calibration sweep (velocities 3–121 resolvable; 1 and 2 are below the noise
floor; 122–123 read the same, as do 124–127).

| Probe | Ref notes | Engine notes | Merged (unresolvable) | Unmatched | Onset max (ms) | Velocity max | Gate max (ms) | Gate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| oct_walk | 32 | 32 | 0 | 0 | 0.74 | 0 | 1.16 | pass |
| oct_idx | 114 | 126 | 0 | 12 | 3.04 | 2 | 4.97 | fail |
| unit200 | 16 | 16 | 0 | 0 | 0.73 | 0 | 0.42 | pass |
| unit66 | 36 | 36 | 0 | 0 | 0.7 | 0 | 0.95 | pass |
| swing | 24 | 24 | 0 | 0 | 1.03 | 2 | 2.44 | fail |
| thru | 24 | 24 | 0 | 0 | 0.69 | 0 | 1.16 | pass |
| outoct | 16 | 24 | 0 | 40 | — | — | — | fail |
| holdon | 36 | 36 | 0 | 0 | 0.73 | 0 | 1.16 | pass |
| syncoff | 26 | 26 | 0 | 0 | 0.71 | 0 | 9.65 | fail |
| toggle | 20 | 20 | 0 | 0 | 0.71 | 0 | 1.16 | pass |
| loopoff | 16 | 16 | 0 | 0 | 0.72 | 0 | 1.16 | pass |
| notelim | 24 | 24 | 0 | 0 | 0.73 | 0 | 1.12 | pass |
| vel0 | 0 | 16 | — | — | — | — | — | fail |
| gate200 | 16 | 16 | 0 | 0 | 0.98 | 2 | 9.54 | fail |
| accent_hi | 25 | 61 | 0 | 36 | 1.0 | 1 | 1.62 | fail |
| accent_lo | 27 | 61 | 0 | 80 | 0.65 | 0 | 1.38 | fail |
| sfx | 6 | 20 | 0 | 26 | — | — | — | fail |
| soul_c | 4 | 36 | 0 | 40 | — | — | — | fail |
| soul_dm7 | 4 | 27 | 0 | 31 | — | — | — | fail |
| rnb3 | 40 | 40 | 0 | 0 | 0.68 | 1 | 1.76 | pass |
| pf2 | 24 | 41 | 0 | 65 | — | — | — | fail |
| up | 32 | 32 | 0 | 0 | 0.69 | 0 | 1.16 | pass |
| up_b | 32 | 32 | 0 | 0 | 0.76 | 0 | 1.16 | pass |
| udb | 32 | 32 | 0 | 0 | 0.77 | 0 | 1.23 | pass |
| up2 | 32 | 32 | 0 | 0 | 0.72 | 0 | 1.22 | pass |
| pf1 | 25 | 32 | 0 | 21 | 4.03 | 21 | 132.59 | fail |
| pf3 | 33 | 44 | 0 | 75 | 0.0 | 15 | 239.1 | fail |
| cr | 77 | 86 | 0 | 105 | 14.02 | 21 | 472.05 | fail |
| bal3 | 14 | 13 | 0 | 1 | 2.33 | 4 | 11.08 | fail |
| uprate | 24 | 24 | 0 | 0 | 0.74 | 0 | 2.26 | pass |
| upthru | 24 | 24 | 0 | 0 | 0.74 | 1 | 1.16 | pass |
| uprel | 15 | 15 | 0 | 0 | 0.68 | 0 | 3.26 | pass |
| outoct_hi | 24 | 24 | 0 | 0 | 0.72 | 0 | 1.16 | pass |
| pf1_hi | 33 | 32 | 0 | 3 | 6.06 | 25 | 1771.94 | fail |
| pf3_hi | 43 | 44 | 0 | 5 | 6.17 | 27 | 1506.1 | fail |
| pf2_hi | 39 | 41 | 0 | 6 | 5.02 | 25 | 1503.4 | fail |
| cr_hi | 67 | 86 | 0 | 77 | 1.57 | 35 | 359.58 | fail |
| bal3_hi | 13 | 13 | 0 | 0 | 1.4 | 4 | 3.91 | fail |
| syn_hi | 77 | 84 | 0 | 7 | 2.97 | 2 | 4.15 | fail |
| oct_idx_hi | 111 | 126 | 0 | 15 | 3.39 | 2 | 4.74 | fail |
| vellim_out | 3 | 3 | 0 | 0 | 0.39 | 0 | 6.1 | fail |
| vellim_in | 16 | 16 | 0 | 0 | 0.74 | 0 | 1.16 | pass |
| swing_only | 24 | 24 | 0 | 0 | 0.93 | 2 | 3.34 | fail |
| q50 | 61 | 69 | 0 | 78 | 10.61 | 47 | 64.11 | fail |
| up_2keys | 16 | 16 | 0 | 0 | 0.71 | 0 | 1.14 | pass |
| syncq | 24 | 24 | 0 | 0 | 0.71 | 0 | 1.16 | pass |
| house_acc | 17 | 17 | 0 | 0 | 0.61 | 2 | 2.28 | fail |
| house_noacc | 17 | 17 | 0 | 0 | 0.73 | 1 | 1.65 | pass |
| house_late | 21 | 22 | 0 | 1 | 0.79 | 2 | 1.65 | fail |
| sfx_nc | 3 | 6 | 0 | 7 | 0.0 | 14 | 506.96 | fail |
| sfx_off | 3 | 3 | 0 | 0 | 7.82 | 12 | 19.9 | fail |
| soul_c2 | 36 | 36 | 0 | 0 | 0.95 | 1 | 2.07 | pass |
| soul_dm7_2 | 36 | 27 | 0 | 27 | 0.92 | 1 | 13.95 | fail |
| soul_g7_2 | 36 | 36 | 0 | 72 | — | — | — | fail |

Reading the failures (other than the known differences above):

- **Measurement limits, not differences:** `syncoff` and `vellim_out` fail only on the gate of a note
  ended by a key release, whose time the recorder knows only within an I/O buffer (9.7 and 6.1 ms);
  `vel0` plays velocity 1, below the analysis floor; the first-batch probes in the low register
  (`outoct`, `pf1`–`pf3`, `cr`, `bal3`, `soul_*`, `accent_*`, `sfx`) cannot separate neighbouring keys
  and were re-recorded an octave or more higher (`*_hi`, `soul_*_2`, `house_*`, `sfx_nc`);
  `oct_idx`, `oct_idx_hi` and `syn_hi` leave 7–15 short notes unresolved where a quiet short note
  follows a loud one on the same key within ~15 ms (checked by hand at 0.81 s of `oct_idx`: the
  reference does play the note).
- **Random:** `cr`, `cr_hi`, `q50` use random tracks (KD13).
- **Open:** KD14–KD17, and style tracks (KD1).

## 11. Decisions for Jacob

1. Proposed measurement gate (section 6): 2 ms onset, keys exact, velocity ±1, gate 5 ms.
2. Sustain pedal: latches arp keys and is not forwarded while the arp owns them (section 4.1).
3. Style tracks (R25) ship approximate at first, flagged in the converter report, rather than
   being left out.
4. Velocity output resolution: the plugin computes rates at 16-bit velocity resolution and sends
   CLAP velocities at that resolution; MIDI-only targets get 7-bit.
5. Phrase push over in-band SysEx (section 3) as the run-time interface for generated phrases.
6. Performance-level arp settings: static values folded into each part by the converter; values
   that scenes change become Performance controls.
7. Name of the format and extension: `oscairn-phrase` / `.ophr`.
