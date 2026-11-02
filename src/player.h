// SPDX-License-Identifier: MIT
// The phrase player: held keys in, timed notes and controllers out. Real-time safe: fixed storage,
// no allocation, locks or I/O after construction. Deterministic for a given input, clock and seed.
#pragma once
#include <cstdint>

#include "phrase.h"

namespace arp {

constexpr int kSlots = 8;
constexpr int kMaxHeld = 32;
constexpr int kMaxSounding = 256;
constexpr int kMaxList = kMaxTemplate + kMaxHeld;

enum class HoldMode : uint8_t { SyncOff, Off, On };
enum class KeyMode : uint8_t { Sort, Thru, Direct, SortDirect, ThruDirect };
enum class VelocityMode : uint8_t { Original, Thru };
enum class TriggerMode : uint8_t { Gate, Toggle };
enum class ChangeTiming : uint8_t { Realtime, Measure };

// Unit Multiply steps; the clock runs at kUnitTwelfths[i] / 12 of the phrase's own speed.
constexpr int kUnitPercent[10] = {50, 66, 75, 100, 133, 150, 200, 266, 300, 400};
constexpr int kUnitTwelfths[10] = {6, 8, 9, 12, 16, 18, 24, 32, 36, 48};

struct Settings {
    bool on = true;
    HoldMode hold = HoldMode::Off;
    KeyMode keyMode = KeyMode::Sort;
    VelocityMode velocityMode = VelocityMode::Original;
    TriggerMode trigger = TriggerMode::Gate;
    ChangeTiming changeTiming = ChangeTiming::Measure;
    bool loop = true;
    bool playOnly = false;      // played keys never sound themselves
    int unit = 3;               // index into kUnitPercent; 3 = 100 %
    int quantizeTicks = 120;    // grid at 480 per quarter: 60, 80, 120, 160, 240, 320, 480
    int quantizeStrength = 0;   // percent
    int swing = 0;              // ticks at 480 per quarter, -120..120
    int velocityRate = 100;     // percent 0..200
    int gateRate = 100;         // percent 0..200
    int octaveRange = 0;        // -3..3
    int outputOctave = 0;       // -10..10
    int noteLow = 0, noteHigh = 127;      // low > high: the keys outside (high, low)
    int velocityLow = 1, velocityHigh = 127;
    int accentThreshold = 0;    // 0 off, else 1..127
    bool accentQuantize = true;
    bool randomSfx = true;
    int sfxVelocityOffset = 0;  // -64..63
    bool sfxKeyVelocity = true; // SFX take the released key's velocity instead of a random one
    bool fixedSdBd = false;
    int syncQuantize = 0;       // ticks at 480 per quarter, 0 = start at the key
    int slot = 0;               // 0..7, the selected phrase
    int slotVelocityOffset[kSlots] = {};  // percent -100..100
    int slotGateOffset[kSlots] = {};
};

struct Output {
    enum Type : uint8_t { NoteOn, NoteOff, Control, Bend, Pressure };
    uint32_t frame;
    Type type;
    uint8_t channel;
    uint8_t key;        // notes; Control: controller number
    uint8_t value;      // Control/Pressure value; notes: 7-bit velocity
    uint16_t velocity;  // notes: 16-bit velocity (1..65535)
    int16_t bend;
    int32_t id;         // notes: identity of the output note
    int32_t inputId;    // a played key passed through: its input identity, else -1
};

// Receives output events in time order. Returning false means the destination is full; the player
// then reports overflowed() and the caller recovers (ends everything).
struct Sink {
    virtual bool push(const Output& event) = 0;
    virtual ~Sink() = default;
};

struct Clock {
    double tempo = 120;     // quarter notes per minute
    double sampleRate = 48000;
    bool hasBeats = false;  // the host transport plays with a beats timeline
    double beats = 0;       // its position in quarter notes at the block start
};

class Player {
public:
    Player();
    // Phrase storage belongs to the caller and must stay valid until replaced.
    void setPhrase(int slot, const Phrase* phrase);
    void setSettings(const Settings& s);
    const Settings& settings() const { return settings_; }
    void seed(uint32_t s) { random_ = s ? s : 1; }

    // A block: begin, then input in frame order, then end.
    void begin(const Clock& clock, uint32_t frames, Sink& sink);
    void keyOn(uint32_t frame, int channel, int key, int velocity, int32_t inputId);
    void keyOff(uint32_t frame, int channel, int key, int32_t inputId);
    void sustain(uint32_t frame, int channel, bool down);
    void advance(uint32_t frame) { advanceTo(frame); }  // render up to `frame` before passing other input on
    void end();

    // The host jumped: a running phrase starts again (at the next Sync Quantize line, if set).
    void relocate(uint32_t frame);
    // Ends every sounding note at `frame` and forgets held keys (stop, reset, choke all).
    void panic(uint32_t frame);
    bool overflowed() const { return overflow_; }
    // The output identity of a played key passed through (matching the input id, or channel and key
    // when the id is -1); -1 if it is not sounding.
    int32_t passThroughId(int32_t inputId, int channel, int key) const;
    bool idle() const;

private:
    struct Held { uint8_t key, velocity, channel; uint32_t order; bool down, latched; };
    struct Sounding { uint8_t key, channel; int32_t id, inputId; double offTick; bool used, passThrough, waitPedal; };
    struct Sched { double tick; int event; int track; };
    struct Entry { uint8_t key; uint8_t velocity; bool absolute; };
    struct Walk { int step = 0, octave = 0; bool down = false; bool started = false; };

    void advanceTo(uint32_t frame);
    void playEvent(int trackIndex, const Event& e, double clockTick, uint32_t frame);
    void playStyle(const Track& t, const Event& e, int channel, double clockTick, uint32_t frame, int fixedKey = -1);
    void playStyleGroup(int trackIndex, const int* events, int count, double clockTick, uint32_t frame);
    bool trackPlays(const Track& t) const;
    int buildList(const Track& t, Entry* out, Entry& first) const;
    void sendNote(const Track& t, uint32_t frame, int channel, int key, int velocity7, uint32_t gate, double clockTick);
    void noteOn(uint32_t frame, int channel, int key, uint16_t velocity, double offTick, int32_t inputId, bool pass);
    void noteOffAt(uint32_t frame, int index);
    void startPhrase(uint32_t frame);
    void stopArp(uint32_t frame);
    void releaseKeysOff(uint32_t frame);
    void playSfx(uint32_t frame, int channel, int velocity);
    void rebuildSchedule();
    void detectChord();
    bool emit(const Output& o);
    bool inNoteRange(int key) const;
    bool inVelocityRange(int velocity) const;
    double tickAt(uint32_t frame) const { return clockTick_ + ticksPerFrame_ * frame; }
    uint32_t frameAt(double tick) const;
    int arpKeysDown() const;
    int heldCount() const { return heldCount_; }
    uint32_t nextRandom();

    Settings settings_;
    const Phrase* slots_[kSlots] = {};
    const Phrase* phrase_ = nullptr;
    int pendingSlot_ = -1;

    Held held_[kMaxHeld];
    int heldCount_ = 0;
    uint32_t order_ = 0;
    bool pedal_[16] = {};
    bool down_[16][128] = {};  // arp-range keys physically down, toggling ones included
    int downCount_ = 0;
    int lastChannel_ = 0;
    int chordRoot_ = 0, chordType_ = 34, chordBass_ = 0;  // 34: no chord

    Sounding sounding_[kMaxSounding];
    int32_t nextId_ = 0;

    Sched sched_[kMaxEvents];     // normal tracks
    int schedCount_ = 0;
    Sched accentSched_[kMaxEvents];
    int accentCount_ = 0;
    bool scheduleDirty_ = true;

    double clockTick_ = 0;       // arp clock at the block start, phrase ticks scaled by unit multiply
    double ticksPerFrame_ = 0;
    double hostBeats_ = 0;
    bool hostHasBeats_ = false;
    double beatsPerFrame_ = 0;
    uint32_t blockFrames_ = 0;
    uint32_t cursorFrame_ = 0;
    double origin_ = 0;          // clock tick of the current pass's start
    double startAt_ = -1;        // a start waiting for its quantize line
    double nextPos_ = 0;         // phrase position still to play in this pass
    bool running_ = false;       // phrase position advancing
    bool audible_ = false;       // notes sounding (Sync-Off runs silently)
    bool toggledOn_ = false;
    int octaveStep_ = 0;         // semitones added this pass by the octave range
    int accent_ = 0;             // 0 none, 1 waiting to start, 2 playing
    double accentAt_ = 0;        // clock tick the accent pass starts
    double accentPos_ = 0;       // accent position still to play
    double muteUntil_ = -1;      // an accent started mid-phrase: normal-track notes before this clock tick stay silent
    Walk walk_[kMaxTracks];
    uint32_t random_ = 0x2545F491u;

    Sink* sink_ = nullptr;
    bool overflow_ = false;
};

}  // namespace arp
