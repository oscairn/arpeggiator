// SPDX-License-Identifier: MIT
#include "record.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace arp {

void Recorder::keyOn(double tick, int key, int velocity) {
    if (!armed_ || done_ || key < 0 || key > 127) return;
    if (!started_) { started_ = true; origin_ = tick; for (auto& o : open_) o = 0; }
    const double t = tick - origin_;
    if (t >= length_ || count_ == kMaxRecorded) return;
    if (open_[key]) keyOff(tick, key);
    notes_[count_] = {uint32_t(t), 0, uint8_t(key), uint8_t(std::clamp(velocity, 1, 127))};
    open_[key] = ++count_;
}

void Recorder::keyOff(double tick, int key) {
    if (!started_ || done_ || key < 0 || key > 127 || !open_[key]) return;
    RecordedNote& n = notes_[open_[key] - 1];
    const double end = std::min<double>(tick - origin_, length_);
    n.gate = uint32_t(std::max(1.0, end - n.tick));
    open_[key] = 0;
}

bool Recorder::advance(double tick) {
    if (!started_ || done_ || tick - origin_ < length_) return false;
    for (int k = 0; k < 128; ++k) if (open_[k]) keyOff(origin_ + length_, k);  // still held: up to the end
    done_ = true;
    armed_ = false;
    return true;
}

std::string recordedPhrase(const RecordedNote* notes, int count, uint32_t length, RecordMode mode, const char* name) {
    std::vector<RecordedNote> v(notes, notes + count);
    std::stable_sort(v.begin(), v.end(), [](const RecordedNote& a, const RecordedNote& b) { return a.tick < b.tick || (a.tick == b.tick && a.key < b.key); });
    // The chord: the notes that start at the first tick (played within 30 ticks of it).
    std::vector<int> chord;
    for (const auto& n : v) if (n.tick <= (v.empty() ? 0 : v[0].tick) + 30) chord.push_back(n.key);
    std::sort(chord.begin(), chord.end());
    const int root = chord.empty() ? 60 : chord[0];
    std::string out;
    char line[160];
    std::snprintf(line, sizeof line, "oscairn-phrase 1\nname \"%s\"\nppq 480\nlength %u\nmeter 4 4\ntrack\n  select index\n  root %d\n",
                  name, length, root);
    out += line;
    for (const auto& n : v) {
        char src[24];
        int pos = -1;
        if (mode == RecordMode::Chord)
            for (size_t i = 0; i < chord.size() && i < 16; ++i)
                if (chord[i] % 12 == n.key % 12) { pos = int(i); break; }
        if (mode == RecordMode::Fixed) std::snprintf(src, sizeof src, "k%d", n.key);
        else if (pos >= 0) {
            const int oct = (n.key - chord[size_t(pos)]) / 12;  // same pitch class: an exact multiple
            if (oct) std::snprintf(src, sizeof src, "p%d%+d", pos, std::clamp(oct, -10, 10));
            else std::snprintf(src, sizeof src, "p%d", pos);
        } else std::snprintf(src, sizeof src, "r%d", n.key);
        std::snprintf(line, sizeof line, "  note %u %s v%d g%u\n", n.tick, src, n.velocity, std::max(1u, n.gate));
        out += line;
    }
    out += "end\n";
    return out;
}

}  // namespace arp
