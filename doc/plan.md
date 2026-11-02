# Oscairn Arpeggiator: plan

Status: 2026-11-01, first draft. Owner: Jacob Schooley.

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
| R1 | A phrase has up to 16 tracks that play together; each track sounds only while the number of held keys is within its own minimum and maximum. Factory phrases use this to give 1, 2, 3 and 4+ held keys different patterns | To measure |
| R2 | A track's note list is its template notes (each either following the chord or absolute) followed by the held keys, optionally without the first held key; or only the held keys | To measure |
| R3 | Held keys are listed by pitch (Sort) or in the order played (Thru); Direct passes the played keys through; Direct alone plays only the phrase's controllers | Manual; to measure |
| R4 | Template notes that follow the chord move with the lowest held key and snap their 2nd, 3rd/4th, 5th and 6th/7th degrees to the held chord's intervals | To measure |
| R5 | Twelve selection rules: by position (none, last or first when the list is shorter), bouncing and cycling positions, walkers up, down, up-down (ends once or twice) and random over 1–4 octaves, and style transposition of literal pitches | To measure |
| R6 | Position notes carry an octave shift of −4 … +3 | To measure |
| R7 | Octave Range moves the whole phrase one octave further on each repetition until the range is reached, then starts over; for walkers it widens the walked octaves | To measure |
| R8 | Output Octave Shift moves every chord-relative note; absolute template notes stay | To measure |
| R9 | Velocity: the phrase's (Original) or the played key's (Thru), times Velocity Rate plus the slot offset; 0 becomes 1, above 127 becomes 127 | Manual; to measure |
| R10 | Gate: the phrase's gate times Gate Rate plus the slot offset; 0 becomes 1 | Manual; to measure |
| R11 | Unit Multiply scales event times and gates by 50, 66.7, 75, 100, 133.3, 150, 200, 266.7, 300 or 400 % | Manual; to measure |
| R12 | Quantize moves events toward the Quantize Value grid by Strength; Swing delays the grid's even-numbered positions | Manual; to measure |
| R13 | Hold Off stops at the last release; On keeps the chord until the next gesture replaces it; Sync-Off keeps the phrase position running silently | Manual; to measure |
| R14 | Trigger Toggle: a gesture starts the arp, the next stops it | Manual; to measure |
| R15 | Loop Off: one pass per gesture | Manual; to measure |
| R16 | Note and velocity limits decide which keys feed the arp; others play normally; Low above High selects the outer ranges | Manual; to measure |
| R17 | A played velocity at or above the Accent threshold switches to the phrase's accent tracks for one pass, at the quantize grid if Accent Start Quantize is on | Manual; to measure |
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
when the transport provides one (RFC 0012: Oscairn's transport, internal or following MIDI clock or
Link), so every part's arpeggiator lines up. A playing-to-stopped transport transition ends all
sounding notes. Unit Multiply scales the arp clock against the tempo.

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
Slot 1–8 Gate Offset. Phrase slots live in the state.

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
| M5 | Record mode, phrase editor operations, asset references | Tests; editor description draft agreed with Oscairn |
| M6 | Motion-sequencer lanes on the same engine | Separate plan |

## 8. Decisions for Jacob

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
