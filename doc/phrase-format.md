# Phrase format (`oscairn-phrase 1`)

A phrase is a short looped pattern of notes and controllers whose notes may refer to the keys being
held. Files use the extension `.ophr`, UTF-8 text, one statement per line. `#` starts a comment;
blank lines are ignored; words are separated by spaces or tabs; a quoted string keeps its spaces.

The first statement is `oscairn-phrase 1`. A reader refuses any other version. Version 1 is
frozen: later versions add statements, and a reader that does not know a statement refuses the file
rather than guessing.

```
oscairn-phrase 1
name "Up and down"
ppq 480
length 1920
meter 4 4

track
  select updown
  octaves 2
  note 0 p0 v100 g60
  note 120 p0 v90 g60
  note 240 p0 v95 g60
  note 360 p0 v90 g60
end
```

## Header

| Statement | Meaning | Default |
| --- | --- | --- |
| `name "TEXT"` | Display name, up to 63 bytes | empty |
| `author "TEXT"` | Who made it, up to 63 bytes | empty |
| `tags "TEXT"` | Comma-separated tags for library search, up to 127 bytes | empty |
| `ppq N` | Ticks per quarter note, 24–3840 | 480 |
| `length N` | Loop length in ticks | 1920 |
| `meter N D` | Meter, for bar-based changes | 4 4 |
| `sfx KLO KHI VLO VHI GATE` | A random-SFX range (up to 8): on a key release, a key from KLO–KHI at a velocity from VLO–VHI, GATE ticks long | none |
| `track` … `end` | A track (up to 16). All tracks play together | |

## Tracks

| Statement | Meaning | Default |
| --- | --- | --- |
| `select RULE` | How a note event picks keys from the note list (below) | `index` |
| `keys held` / `keys template+held` / `keys template+upper` | The note list: the held keys; the template notes then the held keys; the template notes then the held keys except the first | `held` |
| `template KEY NOTE…` | Template notes written against KEY: `c64` follows the chord, `a36` is absolute | none |
| `held MIN MAX` | The track plays notes only while MIN–MAX keys are held (`any` for no maximum) | `1 any` |
| `accent` | The track plays only during an accent pass | |
| `octaves N` | Walkers: octaves walked, 1–4 | 1 |
| `lift N` | `index-last`, `index-first`, `bounce`, `bounce2`, `cycle`: an event whose position is at least N and whose octave is 0 plays an octave up when it falls back to position 0 (or to the first key) | none |
| `root KEY` | Reference key for `r` notes | 60 |
| `channel N` | Output channel 1–16; 0 = the channel of the played keys | 0 |
| `randomize KLO KHI VLO VHI DKLO DKHI DVLO DVHI` | Notes inside the key and velocity window move by a random DKLO–DKHI semitones and DVLO–DVHI velocity steps (up to 4 lines; the first matching applies) | none |
| `chords ROOTS TYPES` | Chord filter, hex masks: bit r of ROOTS for root r (C = 0), bit t of TYPES for chord type t (table below). Controllers, and style notes, play only while the held chord passes | all |
| `style ROOT TYPE` | Style tracks: the chord the source pitches were written for | `0 2` (C maj7) |
| `style-range LO HI` | Style tracks: source pitches below, inside and above this range use the `low`, `mid` and `high` rules | `0 127` |
| `rule low|mid|high T TABLE BASS HIGHKEY LO HI RETRIGGER` | Style transposition rule for a range: T 0 root transposition, 1 root fixed, 2 guitar; TABLE 0–10; BASS 1 follows an on-bass chord's bass; HIGHKEY 0–11 is where root transposition wraps down; result range LO–HI (bit 7 set on either: mute outside instead of folding); RETRIGGER 0–4 for chord changes | `0 0 0 6 0 127 1` |

### Events

Events are in tick order within a track; ticks are from the loop start.

| Statement | Meaning |
| --- | --- |
| `note TICK SOURCE vVEL gGATE` | A note: velocity 1–127, gate in ticks |
| `cc TICK NUMBER VALUE` | A control change |
| `bend TICK VALUE` | Pitch bend, −8192 … 8191 |
| `pressure TICK VALUE` | Channel pressure |

Note sources:

| Source | Meaning |
| --- | --- |
| `k60` | Key 60, whatever is held (on `style` tracks: the source pitch to transpose) |
| `r60` | Key 60 moved by (lowest held key − track root) |
| `p2`, `p2+1`, `p0-2` | Position 2 of the note list, plus octaves (−4 … +3 for converted phrases, −10 … +10 accepted) |
| `p2/k64` | Position 2 if the list has it, otherwise key 64 |

## Selection rules

`n` is the number of entries in the note list; `pos` the event's position.

| `select` | Rule |
| --- | --- |
| `index` | entry `pos`; nothing if `pos ≥ n` (or the fallback key) |
| `index-last` | entry `min(pos, n−1)` |
| `index-first` | entry `pos`; if `pos ≥ n`, the first held key (even when template notes come first) |
| `bounce` | `pos` walks the list up and down, ends once: with n=3, 0 1 2 1 0 1 … |
| `bounce2` | the same, ends twice: 0 1 2 2 1 0 0 … |
| `cycle` | entry `pos mod n` |
| `up`, `down` | walkers: each note event plays the next entry, positions ignored, octaves applied; after the last entry the walk moves an octave (up to `octaves` and the Octave Range), then starts over |
| `updown`, `updown2` | walkers up then down, the turning entries once / twice |
| `random` | a random entry in a random octave of the walked span |
| `style` | the source pitch moves to the held chord by the track's rules |

Walkers keep their place across loops and start over when the arpeggio starts.

**Chord-following template notes** (`c` notes) move by (lowest held key − template KEY). Each
degree the held chord lacks then moves to the nearest degree it has within the same group: {♭2, 2},
{♭3, 3, 4}, {♭5, 5, ♭6}, {6, ♭7, 7, octave}. Ties go down in the second group and up elsewhere. A
group in which the chord has no degree leaves the note alone. Degrees are measured from the lowest
held key.

**Octave Range** (a playback setting, not part of the file): on non-walker tracks the whole phrase
moves one octave further on each loop until the range is reached, then starts again; on walker
tracks it widens the walked span. Absolute template notes never move.

## Chord types

Used by `chords`, `style`: 0 maj, 1 6, 2 maj7, 3 maj7♯11, 4 add9, 5 maj7(9), 6 6(9), 7 aug, 8 m,
9 m6, 10 m7, 11 m7♭5, 12 m(9), 13 m7(9), 14 m7(11), 15 m(maj7), 16 m(maj7)(9), 17 dim, 18 dim7,
19 7, 20 7sus4, 21 7♭5, 22 7(9), 23 7♯11, 24 7(13), 25 7(♭9), 26 7(♭13), 27 7(♯9), 28 maj7aug,
29 7aug, 30 1+8, 31 1+5, 32 sus4, 33 1+2+5, 34 cancel (no chord).

## Plugin state

The CLAP state is text:

```
oscairn-arpeggiator 1
param ID VALUE        # one line per parameter
asset N LIBRARY SHA256 PATH   # optional: the library asset slot N's phrase came from
slot N BYTES          # then exactly BYTES bytes of phrase text and a newline
```

Slots without a `slot` entry are empty.

An `asset` line names the Oscairn library asset (RFC 0009) a slot's phrase is a copy of: N is the
slot (0–7), LIBRARY the library ID (`user` for the user library; no spaces, up to 63 bytes),
SHA256 the file's 64 lowercase hex digits or `-` when not known, and PATH the rest of the line
after one space: the path in the library, which may contain spaces (up to 1023 bytes). Example:
`asset 0 user 68dcea002e7236558e1afcb3655e0a55f65ee550063d92cf14a380778348bf19 68dcea002e723655/MA_SynArp21.ophr`.
The plugin declares each slot's asset to the host as optional (`oscairn.asset-dependencies/draft-2`;
slots sharing one asset list it once). On loading the state it asks the host for the file
(`oscairn.assets`): found, the file's phrase is installed in the slot (and an unknown hash is
filled in); missing, the slot plays the phrase embedded in the state and the host is told
through its log. The line is kept on save either way. A phrase loaded, pushed or recorded into
a slot replaces it and drops its `asset` line. A state with an `asset` line but no `slot` text
for that slot plays nothing there when the asset is missing. Parameter IDs are listed in `src/plugin.cpp`
(1 Switch … 28 Slot, 29–36 slot velocity offsets, 37–44 slot gate offsets, 45 Record,
46 Record Bars, 47 Record Convert) and never change.

## Record mode

With **Record** armed, the arpeggiator passes played keys straight through and captures them:
the first key starts a recording of **Record Bars** bars (4/4 at the host tempo); keys still
held at the end last to it. Record then switches itself off and the main thread writes the take
into the selected slot as a phrase, converted like a MIDI clip (**Record Convert**: Fixed,
Relative, or Chord, where the chord is the keys played within 30 ticks of the first one).

## Pushing a phrase at run time

Another plugin or the host can send a phrase on the arpeggiator's note input as a CLAP
`CLAP_EVENT_MIDI_SYSEX` event:

```
F0 7D 4F 50 48 52 SLOT <phrase text bytes> F7
```

(`7D` is the non-commercial manufacturer ID, `4F 50 48 52` is "OPHR", SLOT is 0–7.) The text is
copied on the audio thread and parsed on the main thread; the phrase takes over the slot at the
start of the next block after that (a main-thread round trip, typically a few milliseconds). Only
one push is in flight at a time; a push that arrives while another is being installed is dropped.
SysEx data bytes are 7-bit, which phrase text always is.

## Converter notes

Values the instrument's performance files store for each arpeggio parameter, and the plugin value
they become:

| Performance parameter | Stored values | Plugin parameter |
| --- | --- | --- |
| Arp Hold | 0 Sync-Off, 1 Off, 2 On | Hold (same values) |
| Arp Key Mode | 0–4 | Key Mode (same) |
| Arp Velocity Mode, Trigger Mode, Change Timing | 0, 1 | same |
| Arp Unit Multiply | 0–9 = 50 … 400 %, 10 = Common | Unit Multiply 0–9; Common: the performance's Unit Multiply |
| Arp Quantize Value | 1–7 = 60, 80, 120, 160, 240, 320, 480 | Quantize Value 0–6 (stored − 1) |
| Arpeggio Synchro Quantize Value | 0 Off, 1–7 = 60 … 480 | Sync Quantize (same) |
| Arp Velocity / Gate Time Rate | 0–200 | same, plus the Common offset (stored − 128) |
| Arp n Velocity / Gate Time Rate Offset | 28–228 = −100 … +100 | Slot n offsets (stored − 128) |
| Arp Octave Range | 61–67 = −3 … +3 | Octave Range (stored − 64), plus the Common offset |
| Arp Output Octave Shift | 54–74 = −10 … +10 | Output Octave (stored − 64), plus the Common offset |
| Swing (Part) | 8–248 = −120 … +120 | Swing (stored − 128), plus the Common Swing Offset |
| Arp Random SFX Velocity Offset | 0–127 = −64 … +63 | SFX Velocity Offset (stored − 64) |
| Arp Accent Velocity Threshold | 0 Off, 1–127 | same |
| Note / Velocity Limits, switches | as stored | same |

Each `Arp n Number` names a phrase: preset numbers are factory phrases, user and library numbers are
the user's files.
