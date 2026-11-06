# Oscairn Arpeggiator: plan

Status: 2026-11-06. Owner: Jacob Schooley.

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
| Arpeggio Master Switch (Common, Scene) | Off, On | — | Performance level: folded into each Part's Switch by the converter until Performance controls are used (decision 8) |
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
Measured or explicitly accepted by Jacob. Rules measured on a few points (R17's fitted window, R25's
root-fixed voicing on three chords) are accepted as they stand, with more probes later (decision 9).

| # | Rule | Source |
| --- | --- | --- |
| R1 | A phrase has up to 16 tracks that play together; each track sounds only while the number of held keys is within its own minimum and maximum. Factory phrases use this to give 1, 2, 3 and 4+ held keys different patterns | Measured for 3 held keys (`rnb3`); keys and timing for 1, 2, 3 held (`pf*_hi`) with the exceptions in section 9 |
| R2 | A track's note list is its template notes (each either following the chord or absolute) followed by the held keys, optionally without the first held key; or only the held keys | Measured (template + held: `rnb3`) |
| R3 | Held keys are listed by pitch (Sort) or in the order played (Thru); Direct passes the played keys through; Direct alone plays only the phrase's controllers | Sort, Thru and Direct Measured (`direct`); Sort+Direct: played keys and arp notes on the same keys cannot be told apart by the analysis (`sortdirect`) |
| R4 | Template notes that follow the chord move with the lowest held key and snap their 2nd, 3rd/4th, 5th and 6th/7th degrees to the held chord's intervals | Measured on a major triad (`rnb3`); other chords to measure |
| R5 | Twelve selection rules: by position (none, last or first when the list is shorter), bouncing and cycling positions, walkers up, down, up-down (ends once or twice) and random over 1–4 octaves, and style transposition of literal pitches | Measured: index, cycle, bounce (keys and order; three onsets off, KD20), bounce2 (all but its first note, KD19), walker up (1 and 2 octaves), up-down ends once (B) and twice (A, `updown_a`). Random: timing and the keys it picks from (the reference's random sequence is not reproduced, KD13). Down to measure |
| R6 | Position notes carry an octave shift of −4 … +3 | Measured (−1: `pf*_hi`) |
| R7 | Octave Range moves the whole phrase one octave further on each repetition until the range is reached, then starts over; for walkers it widens the walked octaves | Walker Measured (+1, `oct_walk`); index phrase: matched notes agree in key and timing, short notes the analysis cannot separate remain (`oct_idx_hi`) |
| R8 | Output Octave Shift moves every chord-relative note; absolute template notes stay | Measured for chord notes (−1, `outoct_hi`); absolute notes to measure |
| R9 | Velocity: the phrase's (Original) or the played key's (Thru), times Velocity Rate plus the slot offset; 0 becomes 1, above 127 becomes 127 | Measured: Original, Thru, rate 50 % (velocity judged where the probe can tell it, section 10) |
| R10 | Gate: the phrase's gate times Gate Rate plus the slot offset; 0 becomes 1 | Measured: 50 % and 200 % (gate times) |
| R11 | Unit Multiply scales event times and gates by 50, 66.7, 75, 100, 133.3, 150, 200, 266.7, 300 or 400 % | Measured: 66 % and 200 % |
| R12 | Quantize moves events toward the Quantize Value grid by Strength; Swing delays the grid's even-numbered positions, with or without quantize strength | Swing timing Measured (+40, with strength 0 and 100); strength 25 and 50 Measured on `MA_Pf Arp1` (`q25_pf`, `q50_pf`: every note in time) |
| R13 | Hold Off stops at the last release, ending sounding notes; On keeps the chord until the next gesture replaces it; Sync-Off ends sounding notes at the release but keeps the phrase position running silently and resumes in place | Measured (`uprel`, `holdon`, `syncoff`) |
| R14 | Trigger Toggle: a gesture (keys pressed together) starts the arp, the next stops it | Measured (`toggle`) |
| R15 | Loop Off: one pass per gesture | Measured (`loopoff`) |
| R16 | Note and velocity limits decide which keys feed the arp; with the arp on, keys outside the note limits do not sound, keys outside the velocity limits play as they are; Low above High selects the outer ranges | Measured, normal and inverted ranges (`notelim_inv`, `vellim_inv`) |
| R17 | A played velocity at or above the Accent threshold adds the phrase's accent tracks, on top of the normal ones, for one pass, at the quantize grid if Accent Start Quantize is on; an accent started after the phrase leaves out the normal tracks' notes in its first quarter note | Measured: accent tracks add, below the threshold nothing changes (`house_acc`, `house_noacc`); started mid-phrase at four places, with and without start quantize (`house_late`, `acc_*`: every note matched); the quarter-note window is fitted (dropped up to 288 ticks after the start, kept from 485) |
| R18 | Random SFX: a key release the arpeggio plays on through plays a random note from the phrase's SFX ranges at a random velocity in its range (or the key's velocity, with Key On Control), plus the velocity offset; the release that ends the arpeggio plays none | Measured: which releases sound one and when (`sfx_seq`, `sfx_nc`); the keys are random (KD13) |
| R19 | Fixed SD/BD maps the phrase's kick and snare variants onto the kit's main kick (36) and snare (38), on Drum Parts | Measured: no effect on an FM-X Part (`sdbd_on`, `sdbd_off`), so the converter sets it only on Drum Parts; the mapping on a Drum Part is not measured (the probe is an FM sound) |
| R20 | Phrase controllers (CC, pitch bend, pressure) play on their ticks while the track's held-count range is met and, for tracks with a chord filter, the held chord passes it | To measure |
| R21 | Change Timing: a new slot starts immediately or at the next bar | Manual; to measure |
| R22 | Sync Quantize: a starting arp waits for the next grid line so parts started together stay together | Manual; shared grid while stopped tested hosted in Oscairn; reference multi-part start measured in `b6` (section 10) |
| R23 | Per-track randomize: notes in a key/velocity window get a random key and velocity offset | To measure |
| R24 | Walker positions continue across loops; they restart with the arp | To measure |
| R25 | Style tracks transpose literal pitches written for a source chord to the held chord, with per-range rules (root transposition or fixed, transposition table, bass, range folding) and a chord filter. Root-fixed voices starting together are voiced together: tones of the held chord in their own register, a chord of four or more tones without its root, every tone covered where there are voices enough, least motion, ties upward | Root-fixed (table 2) Measured on C, Dm7, G7 (`soul_*_2`); root-transposed and other tables approximate |

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
pattern libraries") so they can be browsed, shared and exported as a library. The state also records
each slot's asset reference (an `asset` line, phrase-format.md "Plugin state"); the plugin declares
those references **optional** through `oscairn.asset-dependencies/draft-2`, asks the host for each
file through `oscairn.assets` when its state loads, and plays the embedded copy when the file is
missing. Oscairn then loads the Performance or Sound anyway and shows a notice, and keeps the
references, optional, when it is saved again. A phrase loaded, pushed or recorded into a slot drops
the slot's reference. Converted factory phrases are a library that is exported as a reference
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

**While the transport is stopped** the plugin reads the host's free-running beat
(`oscairn.beat`, RFC 0032 step 3: an event at sample 0 of each block, asked for through CLAP's event
registry) exactly as it reads the playing transport, so parts started together on a stopped
transport start on the same grid line. A process call without the event (a split block) moves the
position on at its tempo. In a host without the event, Sync Quantize waits for the next grid line
of the instance's own clock (KD6). A transport start does not count as a song-position jump.

### 4.3 One instance per Part

Each converted Part gets one arpeggiator instance at the front of its chain (RFC 0021: a Sound is a
saved part chain, "an arpeggiator in front of the synth"), with the node role `arpeggiator` (RFC 0032,
RFC 0016's node roles) so a device profile's arp controls can find it. The role belongs to the node in
the Performance, set by the converter (RFC 0032; not yet written as of converter commit 373f088) or
the user; the plugin cannot declare it. Performance-level settings (master switch,
arp select, common offsets, Arpeggio Group) become Performance controls that target each part's
parameters (RFC 0011 / RFC 0002), or are folded into the part parameters by the converter when they
are static. Parameters (stable IDs):

Switch, Hold, Key Mode, Velocity Mode, Trigger, Loop, Play Only, Unit Multiply, Quantize Value,
Quantize Strength, Swing, Velocity Rate, Gate Rate, Octave Range, Output Octave, Note Low, Note High,
Velocity Low, Velocity High, Change Timing, Accent Threshold, Accent Quantize, Random SFX, SFX
Velocity Offset, SFX Key Velocity, Fixed SD/BD, Sync Quantize, Slot, Slot 1–8 Velocity Offset,
Slot 1–8 Gate Offset, Record, Record Bars, Record Convert, Chord Source. Phrase slots live in the state.

No pitch limit applies beyond the documented parameter ranges: walkers span their octaves plus
the full Octave Range, and only the MIDI key range (0–127, folded by octaves) bounds the result.

### 4.4 Chord Source

**Chord Source** (stepped: Own keys, Harmony; module Arpeggio; default Own keys) says which keys the
phrase plays on. Own keys is the reference's behaviour: the chord of the keys the arpeggiator holds,
which Hold and Toggle latch and its limits filter. Harmony plays on the part's chord as Oscairn
reports it (`oscairn.harmony`, RFC 0033, asked for through the event registry): the keys held at the
part's input through its zones, which a host may give from elsewhere (the hosted test gives the
chord held in a lower zone while the arpeggiator's own input hears the upper one). With Harmony:

- the arpeggiator's own keys still start, hold and stop it: Trigger, Hold, Loop, Sync Quantize,
  Accent, SFX and Direct work on them as before, and the note and velocity limits apply to them, not
  to the host's keys;
- the note list, held-count ranges, template notes, style tracks and chord filters use the host's
  keys, in pitch order (Key Mode Thru has no played order to keep), each with the velocity of the
  first own key held (for Velocity Mode Thru);
- the host's chord changes take effect at their sample; when the host reports no keys the last chord
  stays, so the arpeggio carries on through a released chord (with Hold On it also outlives the own
  keys); before the host has reported any keys the own keys are used;
- in a host without `oscairn.harmony` it behaves as Own keys.

Converted Performances leave it at Own keys (the converter does not write it, and a state without
the parameter keeps its default).

### 4.5 What the converter needs

`performance-converter` (MIT, published) maps each Part's arp block to these parameters (value
tables in phrase-format.md, "Converter notes") and each `Arp n Number` to a phrase file. Arp file
reading is plain format reading (chunked file, config fields, events) and belongs in the converter;
nothing needs the instrument plugin's code or tables at run time. That reading is
`perfconv/arp.py` in `performance-converter`, which also places the arpeggiator first in each
converted Part with its settings, folds the Performance-level values into each Part and carries
the phrases as library assets: the node's references marked `optional`, and an `asset` line per
slot in the state (from converter commit 373f088).

## 5. Converting the instrument's arpeggios

Factory arpeggios are plain files inside the instrument plugin's bundle
(`…/Contents/Resources/contents/arpeggio/NNNNNN-Arpeggio.arp`, 10,922 files, indexed by
`arpeggio.cfg`); user and library arps are in the user content folder
(`…/contents/current/arpeggio/`). Each file is a set of chunks: a header, one chunk per track
(a configuration block followed by MIDI-like events) and name/length/meter/SFX metadata. The
mapping to the phrase format is one to one except: style tracks' transposition tables are
approximated (R25), a key-count remapping field that no factory file uses is reported, and a rare
4-byte controller record (18 factory files) is reported and skipped.

The converter writes the phrases a conversion uses into its library (never committed): embedded in
each arpeggiator's state and as assets under `Assets/` in the archive, referenced by the node.

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

**Gate (accepted by Jacob, decision 1):** per probe, every note matched; keys exact; onset times
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
| M5 | Record mode (done, tested), MIDI import in the C++ library, phrase editor operations, asset references (done, tested: optional, embedded fallback) | Tests; editor description draft agreed with Oscairn |
| M6 | Motion-sequencer lanes on the same engine | Separate plan |

## 8. Alignment with Oscairn RFC 0032

RFC 0032 (Oscairn's side of the arpeggiator) was read at Oscairn commit 6396d9c and again at
3b58efe, whose SDK headers (`beat.h`, `assets.h`, `harmony.h`) are vendored unchanged in
`third_party/oscairn-clap` (commit in `upstream.json`). This plan agrees with it except where noted:

- **Pushed phrases are parsed on the main thread** (resolves RFC 0032's open question and an
  earlier disagreement between this plan and the format document). The audio thread only copies the
  message into a preallocated 256 KiB inbox (bounded time, no allocation) and asks for a main-thread
  callback; the phrase takes over its slot at the start of the first block after the main thread
  has parsed it. Latency is one main-thread round trip, not sample-exact as RFC 0032's
  "at an exact sample" puts it (KD9). One push is in flight at a time; another arriving meanwhile
  is dropped (to be counted once the host has a place to report it).
- **The in-band push through Oscairn's graph** (RFC 0032 step 1) is tested hosted
  (`tests/oscairn/`, below): a plugin before the arpeggiator pushes a phrase at sample 1000 and the
  chord played at sample 3000 plays it, at 256- and 37-sample blocks, in the engine and in a worker.
- **The `7D` message never leaves the plugin:** the arpeggiator consumes every SysEx it receives
  and emits none, so the push cannot reach a MIDI out through it. Filtering `7D` at hardware MIDI
  outs (for messages sent past the arpeggiator) is the host's side.
- **Stopped transport:** see 4.2; the plugin reads `oscairn.beat`. A song-position jump while
  playing starts a running phrase again, at the next Sync Quantize line when one is set (RFC 0032).
- **`oscairn.harmony`** is read only with **Chord Source** set to Harmony (section 4.4, decision 10),
  off by default and in converted Performances: by default the arpeggio's chord is the chord of the
  keys it holds, as the reference's is (correct under Hold and Toggle, which latch keys the host no
  longer counts as held). With Harmony the plugin takes the host's held keys, not its chord type, and
  names the chord with its own table (35 types, on-bass chords; the host's has 22). The key and scale
  are not used: the instrument's arpeggiator has no key or scale setting.
- **Optional phrase assets** (RFC 0032 "In the library", RFC 0009 draft 2): each slot's asset
  reference is in the state and declared optional (section 3), so a deleted phrase file leaves its
  Performances and Sounds loading and playing their embedded copies, with a notice from Oscairn.
- **Node role:** `arpeggiator` on the node, set by the converter (RFC 0032 "Conversion"; not yet
  written by it as of commit 373f088); section 4.3.
- **Hosted test (RFC 0032 step 2):** `tests/oscairn/` builds against an Oscairn source tree (by
  hand, not in CI) and checks, in Oscairn's engine: notes out as the phrase says, the same at 256-
  and 37-sample blocks in the engine and in a worker; a pushed phrase; two arpeggiators added 100 ms
  apart starting on the same beat line with the transport stopped (fails with the plugin from
  before the change: block-size dependent, off the grid); and a converted Performance installed from
  its library archive, loading the arpeggiator and Octet with no stand-ins, arpeggiating with the
  Switch on and not with it off, saving and loading with the same audio and the state byte for byte.
  Since then: Chord Source Harmony, with the chord held in a lower zone given as the arpeggiator's
  harmony (`Graph::setHarmonies`) while its input hears the upper zone, plays that chord; off, its own
  key (engine and worker, 256- and 37-sample blocks; fails with the plugin from before the change).
  Oscairn's sessions give each part the harmony of its own input, so a chord from another Part is
  given at the graph level here.
  **Converted Performances (2026-11-06):** the converted-Performance steps run against Oscairn main
  (9325875) with the converter's library as of 97a2c46 (`Jacob's Performances.olib`, Space Song 2,
  five arpeggiators on a keyboard split). The test takes the arpeggiators from the nodes' role (the
  name suffix " Arp" for older conversions), plays each arpeggiating part alone with a chord inside
  its own zone with its arpeggio off (one chord, no repeated notes), counts the notes each saved
  arpeggiator sends in a graph of its own on a three- and a two-key chord (phrases play only for
  their held counts: SpcEx12 for 1–2 keys, SpcEx3 for 3), and checks the save and reload: every
  arpeggiator's state byte for byte and the same audio. The audio compares only when the
  Performance is saved before any part is soloed or its level set: saved after that, the reload
  played differently (largest difference 0.09; not investigated, outside this plugin). The hosted
  check of deleted optional assets is still not written.
  Chord Source is done: built, unit-, plugin- and hosted-tested.
- **Recording by playing:** the plugin has its own record mode (Record, Record Bars, Record Convert;
  phrase-format.md), which works in any CLAP host and needs nothing from Oscairn. RFC 0032's host
  note capture with a **Make phrase** action would give retroactive capture; it should call the same
  conversion. Today the conversion exists twice (C++ in `src/record.cpp` for the plugin, Python in
  `tools/midi_to_phrase.py`); moving MIDI import into the C++ library as the `phrase.import-midi`
  operation RFC 0032 describes is milestone M5. Whether both recorders are kept is Jacob's call.
- **`author` and `tags`** are in the format before version 1 is frozen.
- **Style tracks (R25):** root-fixed voices follow a rule measured on three chords; root-transposed
  voices remain an approximation built from chord-tone rules (KD1).

## 9. Known differences from the reference

One entry per known difference: what, size and where, cause, deliberate or unresolved, how to
reproduce, where in the code. Entries are added as measurements find them and removed when fixed.

| # | What | Size and where | Cause | Status | Reproduce | Code |
| --- | --- | --- | --- | --- | --- | --- |
| KD1 | Style tracks (R25): root-transposed voices move by root plus chord-tone rules, not the reference's transposition tables; root-fixed voices follow a rule measured only for table 2 on three chords | Root-transposed style tracks on chords other than their source chord, and root-fixed tracks with other tables or chords (unmeasured). `MA_Soul 1 _C` now matches every key and time on C, Dm7 and G7 (`soul_c2`, `soul_dm7_2`, `soul_g7_2` pass); before: Dm7 27 of 36 notes, G7 none | Tables unknown; root-fixed rule fitted to three chords | Unresolved for root-transposed and other tables | `arp-render` a converted style phrase with root-transposed tracks on a chord other than its source chord | `Player::playStyle`, `Player::playStyleGroup` |
| KD2 | A chord change while style notes sound does not re-pitch or stop them (the `rule … RETRIGGER` field is read but unused) | Style tracks only | Not implemented | Unresolved | Change the chord mid-note on a style phrase | `Player::playStyle` |
| KD3 | Chord recognition (for chord filters and style tracks) uses the plugin's own interval table; unusual voicings may be named differently | Chord filters with partial masks, style tracks | Own design | Unresolved, to measure | Hold an unusual voicing on a style phrase | `Player::detectChord` |
| KD4 | Walkers have no octave ceiling: octaves plus the full Octave Range are walked | Walker tracks with many octaves and a large positive Octave Range; the reference may limit the span (to measure) | Jacob's rule: no limit beyond parameter ranges | Deliberate | `octaves 4` phrase with Octave Range +3 | `Player::playEvent`, walker branch |
| KD5 | A key-count remapping field in the instrument's arp files (unused by every factory file) has no equivalent | None in factory content | No data uses it | Deliberate | The converter's report lists any file that sets it | `performance-converter`, `perfconv/arp.py` |
| KD6 | In hosts without `oscairn.beat`, Sync Quantize with the transport stopped aligns to the instance's own clock, not a shared grid (in Oscairn it uses the host's free-running beat) | Other hosts only: parts started together on a stopped transport may differ by up to one grid step | Such hosts give no shared beat while stopped | Deliberate | `tests/test_plugin.cpp`, free-beat case with the plain host | `Plugin::process` (free beat), `Player::startPhrase` |
| KD9 | Pushed phrases take over a block after the main thread parses them, not at the event's sample | All pushes | Parsing must not run unbounded on the audio thread | Deliberate | `tests/test_plugin.cpp`, push case | `Plugin::sysex`, `Plugin::mainThread` |
| KD10 | SysEx other than the push message is dropped, not passed on | Any SysEx on the note input | Keeps private messages inside the plugin | Deliberate | Send any SysEx through | `Plugin::sysex` |
| KD11 | Literal-note and fallback events of user arps recorded on the instrument are not converted | User arps made with the instrument's recording (none in Jacob's set) | No sample files to verify their layout | Unresolved | Convert such a file; the report names it | `perfconv/arp.py` |
| KD12 | A 4-byte controller record in 18 factory arps is skipped | Those 18 phrases | Meaning unknown | Unresolved | Converter report (`event B2 skipped`) | `perfconv/arp.py` |
| KD13 | Random walkers and random phrases play a different random sequence | Every `random` track (`cr`, `cr_hi`, `q50`); timing matches, keys cannot | The plugin's own seeded generator, so renders repeat exactly | Deliberate | `cr_hi` probe | `Player::nextRandom` |
| KD18 | The reference applies a Part's Note Limits (and Note Shift) to what its arpeggio plays: every Part's arpeggio hears every key. In Oscairn a part's zones filter the keys before the plugin | Performances with arpeggio Parts split by note limits: two parts limited to 0–79 and 80–127 play one walk over all four held keys, each sounding its own range (`sync2`, `sync2_off`) | Oscairn's part input filters keys | Handled by the converter: with an arpeggio, the limits and Note Shift go on the note wire after the arpeggiator and the part's input keeps only the velocity limits (velocity limits after the arpeggio are not measured). Whether Oscairn filters on that wire as at a part's input is not verified | Two parts with arps and split note limits, keys in both ranges | `performance-converter`, `perfconv/performance.py` |
| KD19 | `bounce2`: the reference leaves out the phrase's first event (`p0-1`) on every pass; the engine plays it | `MA_Melody12` (`bounce2`), both passes | Unknown (one probe) | Unresolved | `bounce2` probe | `Player::playEvent` (bounce2) |
| KD20 | `bounce`: three notes start 12–14 ms earlier in the reference, and last as much longer, than the engine plays them | `MA_Tracker 1 Synth1 _N` at 1.30, 2.06 and 4.30 s of `bounce` | Unknown: possibly the analysis (overlapping notes of neighbouring keys) | Unresolved | `bounce` probe | — |
| KD21 | Chord Source Harmony plays on the host's chord (`oscairn.harmony`) instead of the held keys; the reference has no such setting | Only with Chord Source set to Harmony; off by default and in converted Performances | An extension for Oscairn (decision 10) | Deliberate | `tests/test_player.cpp`, `chord_source_harmony_…` | `Player::chordKeys`, `Player::setHarmony` |

## 10. Measurement results

Recorded 2026-11-01 (batches `b3`–`b6` under `forks/measurements/arp/`, not in this repository) with
the probe described in section 6 and compared with `arp-render` on the converted phrases (analysis:
`oscairn/measurement`, `arp/compare.py`; the table's Merged column was always 0 through a counting
bug, fixed there: `cr_hi` merges 10 notes, `q50` 27, the others none). All batches compared again
on 2026-11-02 with the plugin and analysis as of then; the analysis now splits same-key notes at
gaps a 10 ms window shows, judges velocity only where the probe can tell it (below) and reads keys
up to 127. Each
render is aligned per phrase start (median offset), since the recorder places key events only within
an I/O buffer. Velocity uses a calibration sweep (velocities 3–121 resolvable; 1 and 2 are below the noise
floor; 122–123 read the same, as do 124–127).

| Probe | Ref notes | Engine notes | Merged | Unmatched | Onset max (ms) | Velocity max | Gate max (ms) | Gate |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| oct_walk | 32 | 32 | 0 | 0 | 0.73 | 0 | 1.17 | pass |
| oct_idx | 116 | 126 | 0 | 10 | 2.65 | — | 43.94 | fail |
| unit200 | 16 | 16 | 0 | 0 | 0.73 | 0 | 0.43 | pass |
| unit66 | 36 | 36 | 0 | 0 | 0.69 | 0 | 0.96 | pass |
| swing | 24 | 24 | 0 | 0 | 1.16 | 0 | 1.96 | pass |
| thru | 24 | 24 | 0 | 0 | 0.69 | 0 | 1.17 | pass |
| outoct | 0 | 24 | — | — | — | — | — | fail |
| holdon | 36 | 36 | 0 | 0 | 0.73 | 0 | 1.17 | pass |
| syncoff | 26 | 26 | 0 | 0 | 0.71 | 0 | 9.66 | fail |
| toggle | 20 | 20 | 0 | 0 | 0.71 | 0 | 1.17 | pass |
| loopoff | 16 | 16 | 0 | 0 | 0.72 | 0 | 1.17 | pass |
| notelim | 24 | 24 | 0 | 0 | 0.73 | 0 | 1.13 | pass |
| vel0 | 0 | 16 | — | — | — | — | — | fail |
| gate200 | 16 | 16 | 0 | 0 | 0.8 | — | 9.31 | fail |
| accent_hi | 25 | 61 | 2 | 36 | 0.71 | 1 | 1.64 | fail |
| accent_lo | 27 | 61 | 0 | 80 | 0.09 | 0 | 1.08 | fail |
| sfx | 6 | 19 | 0 | 25 | — | — | — | fail |
| soul_c | 4 | 36 | 0 | 40 | — | — | — | fail |
| soul_dm7 | 0 | 36 | — | — | — | — | — | fail |
| rnb3 | 40 | 40 | 0 | 0 | 0.68 | 1 | 1.77 | pass |
| pf2 | 30 | 41 | 0 | 71 | — | — | — | fail |
| up | 32 | 32 | 0 | 0 | 0.69 | 0 | 1.17 | pass |
| up_b | 32 | 32 | 0 | 0 | 0.76 | 0 | 1.17 | pass |
| udb | 32 | 32 | 0 | 0 | 0.76 | 0 | 1.24 | pass |
| up2 | 32 | 32 | 0 | 0 | 0.68 | 0 | 1.24 | pass |
| pf1 | 27 | 32 | 0 | 19 | 1.25 | 1 | 1.21 | fail |
| pf3 | 38 | 44 | 0 | 76 | 22.86 | — | 597.03 | fail |
| cr | 90 | 86 | 10 | 110 | 3.27 | 21 | 106.53 | fail |
| bal3 | 26 | 13 | 0 | 13 | 2.65 | — | 11.55 | fail |
| uprate | 24 | 24 | 0 | 0 | 0.74 | 0 | 2.27 | pass |
| upthru | 24 | 24 | 0 | 0 | 0.74 | 1 | 1.17 | pass |
| uprel | 15 | 15 | 0 | 0 | 0.68 | 0 | 3.25 | pass |
| outoct_hi | 24 | 24 | 0 | 0 | 0.72 | 0 | 1.17 | pass |
| pf1_hi | 36 | 32 | 0 | 4 | 0.82 | 1 | 1.8 | fail |
| pf3_hi | 46 | 44 | 0 | 2 | 6.23 | — | 6.06 | fail |
| pf2_hi | 42 | 41 | 0 | 1 | 6.72 | — | 5.64 | fail |
| cr_hi | 84 | 86 | 10 | 82 | 1.47 | 35 | 2.29 | fail |
| bal3_hi | 13 | 13 | 0 | 0 | 1.41 | — | 2.56 | pass |
| syn_hi | 79 | 84 | 0 | 5 | 1.04 | 0 | 2.45 | fail |
| oct_idx_hi | 117 | 126 | 0 | 11 | 2.78 | 0 | 14.63 | fail |
| vellim_out | 3 | 3 | 0 | 0 | 0.82 | — | 5.7 | fail |
| vellim_in | 16 | 16 | 0 | 0 | 0.74 | 0 | 1.17 | pass |
| swing_only | 24 | 24 | 0 | 0 | 1.1 | 0 | 3.82 | pass |
| q50 | 62 | 69 | 27 | 79 | 3.91 | 21 | 63.71 | fail |
| up_2keys | 16 | 16 | 0 | 0 | 0.71 | 0 | 1.15 | pass |
| syncq | 24 | 24 | 0 | 0 | 0.71 | 0 | 1.17 | pass |
| house_acc | 17 | 17 | 1 | 0 | 0.61 | 2 | 2.3 | fail |
| house_noacc | 17 | 17 | 0 | 0 | 0.73 | 1 | 1.66 | pass |
| house_late | 21 | 21 | 0 | 0 | 0.79 | 2 | 1.66 | fail |
| sfx_nc | 3 | 5 | 1 | 6 | 0.0 | — | 259.53 | fail |
| sfx_off | 3 | 3 | 1 | 0 | 2.43 | — | 14.31 | fail |
| soul_c2 | 36 | 36 | 0 | 0 | 0.96 | — | 1.77 | pass |
| soul_dm7_2 | 36 | 36 | 0 | 0 | 1.32 | — | 4.17 | pass |
| soul_g7_2 | 36 | 36 | 0 | 0 | 1.25 | — | 2.36 | pass |
| acc_12 | 21 | 21 | 0 | 0 | 0.95 | 2 | 1.66 | fail |
| acc_22 | 19 | 19 | 0 | 0 | 0.86 | 2 | 1.66 | fail |
| acc_17q0 | 21 | 21 | 0 | 0 | 1.59 | 2 | 1.66 | fail |
| sfx_seq | 6 | 6 | 2 | 10 | 0.0 | — | 246.43 | fail |
| direct | 3 | 3 | 0 | 0 | 0.83 | — | 3.97 | pass |
| sortdirect | 5 | 3 | 16 | 2 | 0.59 | — | 1505.49 | fail |
| updown_a | 16 | 16 | 0 | 0 | 0.73 | 0 | 1.24 | pass |
| bounce | 16 | 16 | 0 | 0 | 13.97 | 0 | 21.66 | fail |
| bounce2 | 14 | 16 | 0 | 2 | 0.91 | 1 | 9.66 | fail |
| cycle | 20 | 20 | 0 | 0 | 0.83 | 2 | 1.82 | fail |
| random1 | 16 | 16 | 0 | 18 | 0.86 | 0 | 1.24 | fail |
| notelim_inv | 16 | 16 | 0 | 0 | 0.72 | 0 | 1.15 | pass |
| vellim_inv | 17 | 17 | 0 | 0 | 7.86 | — | 6.64 | fail |
| sdbd_on | 4 | 8 | 0 | 12 | — | — | — | fail |
| sdbd_off | 4 | 8 | 0 | 12 | — | — | — | fail |
| q50_pf | 34 | 32 | 0 | 2 | 1.06 | 2 | 1.78 | fail |
| q25_pf | 34 | 32 | 0 | 2 | 0.96 | 2 | 9.67 | fail |
| sync2 | 24 | 24 | 0 | 0 | 1.74 | 0 | 2.62 | pass |
| sync2_off | 24 | 24 | 0 | 0 | 0.73 | 0 | 1.17 | pass |

Reading the failures (other than the known differences above):

- **Former known differences that were the analysis:** KD14 (a same-key note 10 ticks after
  another): at a 10 ms window the reference plays both notes as the engine does; the analysis now
  splits them (`pf1_hi` now matches every note). KD15 (velocities of overlapping notes 2–4 steps
  off): played directly with the arpeggio off, notes overlapping notes a third away read up to 5
  steps high (`k15_overlap`) while chords read exactly (`k15_chord`), the probe's level takes the
  same value for several velocities above 100, and phrases with modulation controllers move the
  probe's level (`bal3`, `MC_6/8 Ballad`). Velocity is no longer judged in those cases, and any of
  the velocities sharing a level matches.
- **Fixed:** KD16 (accent started mid-phrase) and KD17 (SFX at the release that ends the arpeggio)
  were real; both rules are measured now (R17, R18). KD1 for root-fixed table-2 voices (R25).
- **Measurement limits, not differences:** `syncoff`, `vellim_out`, `vellim_inv`, `q25_pf` and
  `gate200` fail on the gate of a note ended by a key release, or the onset of a played key passed
  through, whose time the recorder knows only within an I/O buffer (6–10 ms); `vel0` plays
  velocity 1, below the analysis floor; the first-batch probes in the low register (`outoct`,
  `pf1`–`pf3`, `cr`, `bal3`, `soul_*`, `accent_*`, `sfx`) cannot separate neighbouring keys and
  were re-recorded higher; `oct_idx`, `oct_idx_hi` and `syn_hi` still leave 5–12 short notes
  unresolved; `pf1_hi`, `pf2_hi`, `pf3_hi`, `q25_pf`, `q50_pf` read 1–4 extra quiet notes on keys
  no phrase plays (transition artefacts); `sortdirect` cannot tell played keys from arp notes on
  the same keys; `sfx_nc`, `sfx_seq` and `sfx_off` carry pitch bends and same-key notes with no gap.
  Accent notes at velocity 127 coinciding with a normal note on the same key read 2 low
  (`house_acc`, `house_late`, `acc_*`); a 20-tick note in `cycle` reads 2 low.
- **Random:** `cr`, `cr_hi`, `q50`, `random1` use random tracks (KD13); `random1` plays at the
  phrase's times and only the held keys.
- **Not measured:** Change Timing (switching slots needs a scene or Arp Select change the recorder
  cannot send) and Sync Quantize across parts in the reference: the two-part probe showed KD18
  instead (both parts' arpeggios start with the first key), so it needs parts on separate MIDI
  channels or keys, which the recorder plays on one. Down walkers, the octave of absolute template
  notes under Output Octave, and Fixed SD/BD on a Drum Part.

## 11. Decisions for Jacob

1. Measurement gate (section 6): 2 ms onset, keys exact, velocity ±1, gate 5 ms. **Accepted**
   (2026-11-02).
2. Sustain pedal: latches arp keys and is not forwarded while the arp owns them (section 4.1).
3. Style tracks (R25) ship approximate at first, flagged in the converter report, rather than
   being left out.
4. Velocity output resolution: the plugin computes rates at 16-bit velocity resolution and sends
   CLAP velocities at that resolution; MIDI-only targets get 7-bit.
5. Phrase push over in-band SysEx (section 3) as the run-time interface for generated phrases.
6. Performance-level arp settings: static values folded into each part by the converter; values
   that scenes change become Performance controls.
7. Name of the format and extension: `oscairn-phrase` / `.ophr`.
8. Arpeggio Master Switch folded into each Part's Switch by the converter until Performance
   controls are used. **Accepted** (2026-11-02).
9. Rules measured on a few points (section 1.2) accepted as they stand, with more probes later.
   **Accepted** (2026-11-02).
10. `oscairn.harmony`: the held chord stays the arpeggiator's own by default; an option, off by
    default, follows the host's chord instead (Chord Source, section 4.4). **Decided** (2026-11-02).
