// SPDX-License-Identifier: MIT
// Record mode: played notes captured on the audio thread, turned into a phrase on the main thread.
#pragma once
#include <cstdint>
#include <string>

namespace arp {

constexpr int kMaxRecorded = 2048;

struct RecordedNote { uint32_t tick, gate; uint8_t key, velocity; };

enum class RecordMode : uint8_t {
    Fixed,     // every note keeps its key (k)
    Relative,  // every note follows the lowest held key (r, root = the lowest note at the start)
    Chord,     // notes of the chord held at the start become positions (p), others follow the lowest key
};

// Capture on the audio thread: fixed storage, no allocation.
class Recorder {
public:
    void arm(uint32_t lengthTicks) { armed_ = true; started_ = false; done_ = false; count_ = 0; length_ = lengthTicks; }
    void disarm() { armed_ = false; }
    bool armed() const { return armed_; }
    bool done() const { return done_; }
    // `tick` is the recording clock (480 per quarter); the first key-on starts it at 0.
    void keyOn(double tick, int key, int velocity);
    void keyOff(double tick, int key);
    // Advances the clock; returns true when the recording just finished.
    bool advance(double tick);
    double origin() const { return origin_; }
    bool started() const { return started_; }
    int count() const { return count_; }
    const RecordedNote* notes() const { return notes_; }
    uint32_t length() const { return length_; }

private:
    RecordedNote notes_[kMaxRecorded];
    int open_[128] = {};  // index + 1 of the sounding note per key
    int count_ = 0;
    uint32_t length_ = 1920;
    double origin_ = 0;
    bool armed_ = false, started_ = false, done_ = false;
};

// The phrase text for recorded notes (main thread).
std::string recordedPhrase(const RecordedNote* notes, int count, uint32_t length, RecordMode mode, const char* name);

}  // namespace arp
