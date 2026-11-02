// SPDX-License-Identifier: MIT
// A phrase: timed events on up to kMaxTracks tracks that play together. Each note names its pitch
// absolutely, relative to the lowest held key, or by its position in the track's note list (template
// notes, then held keys). Fixed capacity, so a phrase can be parsed and swapped without allocating.
// The text form is documented in doc/phrase-format.md.
#pragma once
#include <cstddef>
#include <cstdint>

namespace arp {

constexpr int kMaxTracks = 16;
constexpr int kMaxEvents = 4096;  // per phrase, all tracks together
constexpr int kMaxTemplate = 32;
constexpr int kMaxRandomize = 4;
constexpr int kMaxSfx = 8;
constexpr int kNameSize = 64;
constexpr int kTagsSize = 128;
constexpr int kAnyCount = 128;  // "held MIN any"

enum class Source : uint8_t {
    Key,       // k60: this key (on style tracks: the source pitch to transpose)
    Relative,  // r60: this key moved by (lowest held key - track root)
    Position,  // p2+1: note-list position 2, one octave up
    Fallback,  // p2/k64: position 2 when the list has it, otherwise key 64
};

enum class EventType : uint8_t { Note, Control, Bend, Pressure };

struct Event {
    uint32_t tick = 0;
    uint32_t gate = 0;  // notes: length in ticks
    EventType type = EventType::Note;
    Source source = Source::Key;
    int8_t octave = 0;    // Position/Fallback: octaves added, -4..3 for imported phrases
    uint8_t position = 0; // Position/Fallback: index into the note list, 0..15
    uint8_t key = 0;      // Key/Relative/Fallback key; Control: controller number
    uint8_t velocity = 0; // notes 1..127; Control/Pressure: value
    int16_t bend = 0;     // Bend: -8192..8191
};

// How a track turns a note event into keys from its note list.
enum class Select : uint8_t {
    Index,       // the event's position; none when the list is shorter
    IndexLast,   // ... the last entry when the list is shorter
    IndexFirst,  // ... the first held key when the list is shorter
    Bounce,      // position walks the list up and down, ends played once
    Bounce2,     // ... ends played twice
    Cycle,       // position modulo the list length
    Up,          // walkers: one step per note event, positions ignored, across `octaves`
    Down,
    UpDown,      // turning notes played once
    UpDown2,     // turning notes played twice
    Random,
    Style,       // source pitches transposed to the held chord
};
constexpr int kSelectCount = 12;

enum class Keys : uint8_t {
    Held,           // the note list is the held keys
    TemplateHeld,   // template notes, then the held keys
    TemplateUpper,  // template notes, then the held keys except the first
};

struct TemplateNote { uint8_t key; bool follows; };  // follows: moves with the held chord

struct Randomize { uint8_t keyLow, keyHigh, velLow, velHigh; int8_t keyMin, keyMax, velMin, velMax; };

// Style tracks: how a source note written for (styleRoot, styleType) follows the held chord.
struct StyleRule {
    uint8_t transpose = 0;  // 0 root transposition, 1 root fixed, 2 guitar
    uint8_t table = 0;      // note transposition table 0..10
    bool bass = false;      // follow the bass note of an on-bass chord
    uint8_t highKey = 6;    // 0..11, wrap point for root transposition
    uint8_t low = 0, high = 127;  // result range; bit 7 set: mute instead of folding
    uint8_t retrigger = 1;  // on a chord change: 0 stop, 1-2 bend, 3-4 retrigger
};

struct Track {
    Select select = Select::Index;
    Keys keys = Keys::Held;
    bool accent = false;    // plays only while an accent is active
    uint8_t minHeld = 1, maxHeld = kAnyCount;  // the track's notes play only within this held count
    uint8_t octaves = 1;    // walkers: octaves walked, 1..4
    uint8_t lift = 255;     // IndexLast/IndexFirst/Bounce/Cycle: positions >= lift go up an octave
    uint8_t root = 60;      // reference key for Relative sources
    uint8_t channel = 0;    // 0 = the channel of the played keys, else 1..16
    uint8_t templateKey = 60;
    uint8_t templateCount = 0;
    TemplateNote templ[kMaxTemplate] = {};
    uint8_t randomizeCount = 0;
    Randomize randomize[kMaxRandomize] = {};
    uint16_t rootMask = 0xFFF;           // chord roots that let style notes and controllers play
    uint64_t typeMask = 0x7FFFFFFFFull;  // chord types, 0..34
    uint8_t styleRoot = 0, styleType = 2;
    uint8_t styleLow = 0, styleHigh = 127;
    StyleRule rules[3];                  // source below, inside, above the style range
    int first = 0, count = 0;            // events[first, first+count), sorted by tick
};

struct Sfx { uint8_t keyLow, keyHigh, velLow, velHigh; uint32_t gate; };

struct Phrase {
    char name[kNameSize] = {};
    char author[kNameSize] = {};
    char tags[kTagsSize] = {};  // comma-separated, for library search
    uint32_t ppq = 480;
    uint32_t length = 1920;  // loop length in ticks
    uint8_t meterNum = 4, meterDen = 4;
    int trackCount = 0;
    Track tracks[kMaxTracks];
    int eventCount = 0;
    Event events[kMaxEvents];
    int sfxCount = 0;
    Sfx sfx[kMaxSfx];
    bool hasAccent = false;
};

// Parses the text form. Allocation-free; returns false and a message on the first error.
bool parsePhrase(const char* text, size_t size, Phrase& out, char* error, size_t errorSize);
// Writes the text form; returns the bytes needed (writes at most `size`, NUL-terminated if room).
size_t writePhrase(const Phrase& phrase, char* out, size_t size);

}  // namespace arp
