// SPDX-License-Identifier: MIT
#include "player.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arp {
namespace {

constexpr double kNever = std::numeric_limits<double>::infinity();
int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
int fold(int k) {
    while (k < 0) k += 12;
    while (k > 127) k -= 12;
    return k;
}
uint16_t velocity16(int v) {
    v = clampi(v, 0, 127);
    uint32_t x = uint32_t(v) << 9;
    if (v >= 64) x |= uint32_t(v) << 2 | uint32_t(v) >> 5;  // reach 0xFFFF at 127
    return uint16_t(x);
}

// Chord types 0..33 as pitch-class sets above the root (bit i = i semitones); 34 = no chord.
constexpr uint16_t kChordShapes[34] = {
    0x091, 0x291, 0x891, 0x8D1, 0x095, 0x895, 0x295, 0x111,  // M 6 M7 M7#11 add9 M7(9) 6(9) aug
    0x089, 0x289, 0x489, 0x449, 0x08D, 0x48D, 0x4A9, 0x889,  // m m6 m7 m7b5 m(9) m7(9) m7(11) mM7
    0x88D, 0x049, 0x249, 0x491, 0x4A1, 0x451, 0x495, 0x4D1,  // mM7(9) dim dim7 7 7sus4 7b5 7(9) 7#11
    0x691, 0x493, 0x591, 0x499, 0x911, 0x511, 0x001, 0x081,  // 7(13) 7b9 7b13 7#9 M7aug 7aug 1+8 1+5
    0x0A1, 0x085,                                            // sus4 1+2+5
};

int thirdOf(uint16_t shape) { return shape & 0x010 ? 4 : shape & 0x008 ? 3 : shape & 0x020 ? 5 : -1; }
int fifthOf(uint16_t shape) { return shape & 0x080 ? 7 : shape & 0x040 ? 6 : shape & 0x100 ? 8 : -1; }
int seventhOf(uint16_t shape) { return shape & 0x400 ? 10 : shape & 0x800 ? 11 : shape & 0x200 ? 9 : -1; }

// Chord-following template notes: a degree the held chord lacks moves to the nearest degree it has
// within its group (2nds; 3rds and 4th; 5ths and b6; 6th and 7ths, which may also reach the octave).
int snap(int degree, uint32_t heldMask) {
    static const int groups[4][2] = {{1, 2}, {3, 5}, {6, 8}, {9, 12}};
    for (int g = 0; g < 4; ++g) {
        const int lo = groups[g][0], hi = groups[g][1];
        if (degree < lo || degree > std::min(hi, 11)) continue;
        if (heldMask >> degree & 1) return 0;
        int best = 99;
        for (int d = lo; d <= hi; ++d) {
            if (!(heldMask >> d & 1)) continue;
            const int dist = std::abs(d - degree), bestDist = std::abs(best - degree);
            // Ties go down among 3rds/4th, up elsewhere.
            if (dist < bestDist || (dist == bestDist && (g == 1 ? d < best : d > best))) best = d;
        }
        return best == 99 ? 0 : best - degree;
    }
    return 0;
}

int sdbd(int key) {
    switch (key) {
        case 33: case 35: case 36: return 36;
        case 31: case 34: case 40: return 38;
        default: return key;
    }
}

}  // namespace

Player::Player() {
    for (auto& s : sounding_) s = Sounding{};
}

uint32_t Player::nextRandom() {
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return random_;
}

void Player::setPhrase(int slot, const Phrase* phrase) {
    if (slot < 0 || slot >= kSlots) return;
    slots_[slot] = phrase;
    if (slot == settings_.slot) {
        phrase_ = phrase;
        scheduleDirty_ = true;
        nextPos_ = 0;
        for (auto& w : walk_) w = Walk{};
    }
}

void Player::setSettings(const Settings& s) {
    const bool regrid = s.quantizeTicks != settings_.quantizeTicks || s.quantizeStrength != settings_.quantizeStrength ||
                        s.swing != settings_.swing;
    const int oldSlot = settings_.slot;
    const bool wasOn = settings_.on;
    settings_ = s;
    settings_.unit = clampi(s.unit, 0, 9);
    settings_.slot = oldSlot;
    if (regrid) scheduleDirty_ = true;
    const int want = clampi(s.slot, 0, kSlots - 1);
    if (want != oldSlot) {
        if (s.changeTiming == ChangeTiming::Realtime || !running_) {
            settings_.slot = want;
            phrase_ = slots_[want];
            scheduleDirty_ = true;
            pendingSlot_ = -1;
        } else pendingSlot_ = want;
    }
    if (wasOn && !s.on) {
        running_ = audible_ = false;
        startAt_ = -1;
        accent_ = 0;
        if (sink_) stopArp(cursorFrame_);
    }
}

bool Player::emit(const Output& o) {
    if (overflow_ || !sink_) return false;
    if (!sink_->push(o)) { overflow_ = true; return false; }
    return true;
}

// The first frame at or after `tick`, never before the frames already rendered; may be past the block.
uint32_t Player::frameAt(double tick) const {
    if (ticksPerFrame_ <= 0) return cursorFrame_;
    // The tolerance covers the host beat's resolution (2^-31 beat, up to ~1e-5 frame at slow tempos and high rates).
    const double f = std::ceil((tick - clockTick_) / ticksPerFrame_ - 1e-4);
    if (f < cursorFrame_) return cursorFrame_;
    if (f > 4294967295.0) return 0xFFFFFFFFu;
    return uint32_t(f);
}

bool Player::inNoteRange(int key) const {
    const int lo = settings_.noteLow, hi = settings_.noteHigh;
    return lo <= hi ? key >= lo && key <= hi : key <= hi || key >= lo;
}
bool Player::inVelocityRange(int v) const {
    const int lo = settings_.velocityLow, hi = settings_.velocityHigh;
    return lo <= hi ? v >= lo && v <= hi : v <= hi || v >= lo;
}

int Player::arpKeysDown() const {
    int n = 0;
    for (int i = 0; i < heldCount_; ++i) n += held_[i].down;
    return n;
}

int32_t Player::passThroughId(int32_t inputId, int channel, int key) const {
    for (const auto& s : sounding_)
        if (s.used && s.passThrough && (inputId >= 0 ? s.inputId == inputId : s.channel == channel && s.key == key)) return s.id;
    return -1;
}

bool Player::idle() const {
    if (running_ || heldCount_ || startAt_ >= 0) return false;
    for (const auto& s : sounding_) if (s.used) return false;
    return true;
}

// Root, type and bass of the held keys, for style tracks and chord filters.
void Player::detectChord() {
    uint16_t pcs = 0;
    int lowest = 128;
    for (int i = 0; i < heldCount_; ++i) {
        pcs |= uint16_t(1u << (held_[i].key % 12));
        lowest = std::min<int>(lowest, held_[i].key);
    }
    chordType_ = 34;
    if (!pcs) return;
    chordBass_ = lowest % 12;
    for (int pass = 0; pass < 12; ++pass) {
        const int root = (chordBass_ + pass) % 12;  // prefer the bass as root
        const uint16_t rel = uint16_t(((pcs >> root) | (pcs << (12 - root))) & 0xFFF);
        for (int t = 0; t < 34; ++t)
            if (kChordShapes[t] == rel) { chordRoot_ = root; chordType_ = t; return; }
    }
    chordRoot_ = chordBass_;
    chordType_ = 30;  // unrecognised: treat as the bass alone
}

void Player::rebuildSchedule() {
    scheduleDirty_ = false;
    schedCount_ = accentCount_ = 0;
    if (!phrase_) return;
    const double scale = phrase_->ppq / 480.0;
    const double q = settings_.quantizeTicks * scale;
    const double strength = settings_.quantizeStrength / 100.0;
    const double swing = settings_.swing * scale;
    for (int ti = 0; ti < phrase_->trackCount; ++ti) {
        const Track& tr = phrase_->tracks[ti];
        for (int i = tr.first; i < tr.first + tr.count; ++i) {
            double t = phrase_->events[i].tick;
            if (q > 0) {
                const double k = std::round(t / q);
                t += (k * q - t) * strength;
                if (std::fmod(std::fabs(k), 2.0) == 1.0) t += swing;  // the grid's even-numbered positions
            }
            t = std::max(0.0, t);
            if (tr.accent) accentSched_[accentCount_++] = {t, i, ti};
            else sched_[schedCount_++] = {t, i, ti};
        }
    }
    // std::sort with the event index as tie-break: stable, and unlike std::stable_sort it never allocates.
    auto byTick = [](const Sched& a, const Sched& b) { return a.tick < b.tick || (a.tick == b.tick && a.event < b.event); };
    std::sort(sched_, sched_ + schedCount_, byTick);
    std::sort(accentSched_, accentSched_ + accentCount_, byTick);
}

void Player::begin(const Clock& clock, uint32_t frames, Sink& sink) {
    sink_ = &sink;
    blockFrames_ = frames;
    cursorFrame_ = 0;
    const double ppq = phrase_ ? phrase_->ppq : 480;
    const double tempo = clock.tempo > 0 && std::isfinite(clock.tempo) ? clock.tempo : 120;
    const double rate = clock.sampleRate > 0 && std::isfinite(clock.sampleRate) ? clock.sampleRate : 48000;
    beatsPerFrame_ = tempo / 60.0 / rate;
    ticksPerFrame_ = beatsPerFrame_ * ppq * 12.0 / kUnitTwelfths[settings_.unit];
    hostHasBeats_ = clock.hasBeats && std::isfinite(clock.beats);
    hostBeats_ = clock.beats;
    if (scheduleDirty_) rebuildSchedule();
}

void Player::end() {
    advanceTo(blockFrames_);
    clockTick_ += ticksPerFrame_ * blockFrames_;
    sink_ = nullptr;
}

void Player::noteOffAt(uint32_t frame, int i) {
    Sounding& s = sounding_[i];
    if (!s.used) return;
    s.used = false;
    emit({frame, Output::NoteOff, s.channel, s.key, 0, 0, 0, s.id, s.inputId});
}

void Player::noteOn(uint32_t frame, int channel, int key, uint16_t velocity, double offTick, int32_t inputId, bool pass) {
    if (!pass)  // a repeated key ends the note it is still sounding
        for (int i = 0; i < kMaxSounding; ++i)
            if (sounding_[i].used && !sounding_[i].passThrough && sounding_[i].key == key && sounding_[i].channel == channel)
                noteOffAt(frame, i);
    for (int i = 0; i < kMaxSounding; ++i) {
        if (sounding_[i].used) continue;
        if (nextId_ == std::numeric_limits<int32_t>::max()) nextId_ = 0;
        sounding_[i] = {uint8_t(key), uint8_t(channel), nextId_++, inputId, offTick, true, pass, false};
        const uint8_t v7 = uint8_t(std::max(1, velocity >> 9));
        emit({frame, Output::NoteOn, uint8_t(channel), uint8_t(key), v7, velocity, 0, sounding_[i].id, inputId});
        return;
    }
    overflow_ = true;
}

void Player::startPhrase(uint32_t frame) {
    const double now = tickAt(frame);
    running_ = audible_ = true;
    octaveStep_ = 0;
    accent_ = 0;
    nextPos_ = 0;
    for (auto& w : walk_) w = Walk{};
    startAt_ = -1;
    origin_ = now;
    if (settings_.syncQuantize > 0) {
        // Wait for the next grid line of the host's beat timeline, or of the arp clock without one.
        const double q = settings_.syncQuantize / 480.0;  // in quarter notes
        double wait = 0;
        if (hostHasBeats_) {
            const double beat = hostBeats_ + beatsPerFrame_ * frame;
            wait = (std::ceil(beat / q - 1e-9) * q - beat) / beatsPerFrame_ * ticksPerFrame_;
        } else {
            const double qt = q * (phrase_ ? phrase_->ppq : 480) * 12.0 / kUnitTwelfths[settings_.unit];
            wait = std::ceil(now / qt - 1e-9) * qt - now;
        }
        if (wait > 1e-6) startAt_ = now + wait;
    }
}

void Player::relocate(uint32_t frame) {
    advanceTo(frame);
    if (!running_) return;
    const bool audible = audible_;
    startPhrase(frame);
    audible_ = audible;
}

void Player::stopArp(uint32_t frame) {
    running_ = audible_ = false;
    startAt_ = -1;
    accent_ = 0;
    for (int i = 0; i < kMaxSounding; ++i)
        if (sounding_[i].used && !sounding_[i].passThrough) noteOffAt(frame, i);
}

void Player::panic(uint32_t frame) {
    for (int i = 0; i < kMaxSounding; ++i) noteOffAt(frame, i);
    heldCount_ = 0;
    for (auto& c : down_) for (auto& k : c) k = false;
    downCount_ = 0;
    running_ = audible_ = toggledOn_ = false;
    accent_ = 0;
    startAt_ = -1;
    for (auto& p : pedal_) p = false;
    overflow_ = false;
}

void Player::playSfx(uint32_t frame, int channel, int velocity) {
    if (!phrase_ || !phrase_->sfxCount || !settings_.randomSfx) return;
    const Sfx& s = phrase_->sfx[nextRandom() % uint32_t(phrase_->sfxCount)];
    const int lo = std::min(s.keyLow, s.keyHigh), hi = std::max(s.keyLow, s.keyHigh);
    const int key = lo + int(nextRandom() % uint32_t(hi - lo + 1));
    int v = velocity;
    if (!settings_.sfxKeyVelocity) {
        const int vlo = std::min(s.velLow, s.velHigh), vhi = std::max(s.velLow, s.velHigh);
        v = vlo + int(nextRandom() % uint32_t(vhi - vlo + 1));
    }
    v = clampi(v + settings_.sfxVelocityOffset, 1, 127);
    noteOn(frame, channel, key, velocity16(v), tickAt(frame) + s.gate, -1, false);
}

void Player::keyOn(uint32_t frame, int channel, int key, int velocity, int32_t inputId) {
    advanceTo(frame);
    if (channel < 0 || channel > 15 || key < 0 || key > 127) return;
    lastChannel_ = channel;
    velocity = clampi(velocity, 1, 127);
    const KeyMode km = settings_.keyMode;
    const bool direct = km == KeyMode::Direct || km == KeyMode::SortDirect || km == KeyMode::ThruDirect;
    const bool active = settings_.on && phrase_;
    // With the arpeggio on, keys outside its note limits do not sound, while keys outside its velocity
    // limits play as they are (both measured); with it off every key passes through.
    if (active && !inNoteRange(key)) return;
    const bool arpKey = active && inVelocityRange(velocity);
    if ((!arpKey || direct) && !(settings_.playOnly && settings_.on))
        noteOn(frame, channel, key, velocity16(velocity), kNever, inputId, true);
    if (!arpKey || heldCount_ == kMaxHeld) return;
    // A gesture starts when no arp key is physically down; keys pressed with it belong to it.
    const bool gestureStart = downCount_ == 0;
    if (!down_[channel][key]) { down_[channel][key] = true; ++downCount_; }
    const bool latch = settings_.hold == HoldMode::On || settings_.trigger == TriggerMode::Toggle;
    if (settings_.trigger == TriggerMode::Toggle && gestureStart) {
        toggledOn_ = !toggledOn_;
        if (!toggledOn_) {
            heldCount_ = 0;
            stopArp(frame);
            return;
        }
    }
    if (gestureStart && latch) heldCount_ = 0;  // a new gesture replaces the latched chord
    held_[heldCount_++] = {uint8_t(key), uint8_t(velocity), uint8_t(channel), order_++, true, false};
    detectChord();
    const bool resume = settings_.hold == HoldMode::SyncOff && running_ && settings_.loop;
    if (gestureStart) {
        if (resume) audible_ = true;
        else startPhrase(frame);
    }
    if (phrase_->hasAccent && settings_.accentThreshold > 0 && velocity >= settings_.accentThreshold && accent_ == 0) {
        double at = tickAt(frame);
        if (settings_.accentQuantize && settings_.quantizeTicks > 0) {
            const double q = settings_.quantizeTicks * phrase_->ppq / 480.0;
            const double pos = std::max(0.0, at - origin_);
            at = origin_ + std::ceil(pos / q - 1e-9) * q;
        }
        accent_ = 1;
        accentAt_ = std::max(at, startAt_ >= 0 ? startAt_ : at);
        accentPos_ = 0;
    }
}

void Player::keyOff(uint32_t frame, int channel, int key, int32_t inputId) {
    advanceTo(frame);
    if (channel >= 0 && channel < 16 && key >= 0 && key < 128 && down_[channel][key]) { down_[channel][key] = false; --downCount_; }
    for (int i = 0; i < kMaxSounding; ++i)
        if (sounding_[i].used && sounding_[i].passThrough && sounding_[i].key == key && sounding_[i].channel == channel &&
            !sounding_[i].waitPedal && (inputId < 0 || sounding_[i].inputId == inputId)) {
            if (channel >= 0 && channel < 16 && pedal_[channel]) sounding_[i].waitPedal = true;
            else noteOffAt(frame, i);
            break;
        }
    int found = -1;
    for (int i = 0; i < heldCount_; ++i)
        if (held_[i].down && held_[i].key == key && held_[i].channel == channel) { found = i; break; }
    if (found < 0) return;
    const int velocity = held_[found].velocity;
    held_[found].down = false;
    const bool latch = settings_.hold == HoldMode::On || settings_.trigger == TriggerMode::Toggle;
    if (latch || pedal_[channel]) held_[found].latched = true;
    else {
        for (int i = found; i + 1 < heldCount_; ++i) held_[i] = held_[i + 1];
        --heldCount_;
    }
    detectChord();
    const bool sfx = running_ && audible_;
    releaseKeysOff(frame);
    if (sfx) playSfx(frame, channel, velocity);  // after a stop, so the stop does not cut it
}

// After a release: with nothing left to play, Hold Off stops and Sync-Off goes silent.
void Player::releaseKeysOff(uint32_t frame) {
    if (heldCount_) return;
    if (settings_.hold == HoldMode::SyncOff && settings_.loop) {
        // Silent, but the phrase keeps its place; sounding notes end now, as with Hold Off.
        stopArp(frame);
        running_ = true;
    } else stopArp(frame);
}

void Player::sustain(uint32_t frame, int channel, bool down) {
    advanceTo(frame);
    if (channel < 0 || channel > 15 || pedal_[channel] == down) return;
    pedal_[channel] = down;
    if (down) return;
    for (int i = 0; i < kMaxSounding; ++i)
        if (sounding_[i].used && sounding_[i].passThrough && sounding_[i].waitPedal && sounding_[i].channel == channel)
            noteOffAt(frame, i);
    if (settings_.hold == HoldMode::On || settings_.trigger == TriggerMode::Toggle) return;
    int n = 0;
    for (int i = 0; i < heldCount_; ++i)
        if (held_[i].down || held_[i].channel != channel) held_[n++] = held_[i];
    if (n == heldCount_) return;
    heldCount_ = n;
    detectChord();
    releaseKeysOff(frame);
}

// The track's note list: template notes (moved with the chord or absolute), then held keys.
int Player::buildList(const Track& t, Entry* out, Entry& first) const {
    Held keys[kMaxHeld];
    const int n = heldCount_;
    std::copy(held_, held_ + n, keys);
    const bool thru = settings_.keyMode == KeyMode::Thru || settings_.keyMode == KeyMode::ThruDirect;
    std::sort(keys, keys + n, [thru](const Held& a, const Held& b) {
        return thru ? a.order < b.order : a.key < b.key || (a.key == b.key && a.order < b.order);
    });
    int count = 0;
    if (!n) return 0;
    int lowest = 127;
    for (int i = 0; i < n; ++i) lowest = std::min<int>(lowest, keys[i].key);
    first = {keys[0].key, keys[0].velocity, false};
    if (t.keys != Keys::Held && t.templateCount) {
        uint32_t mask = 1;
        for (int i = 0; i < n; ++i) {
            const int iv = keys[i].key - lowest;
            if (iv > 0) mask |= 1u << ((iv - 1) % 12 + 1);
        }
        for (int i = 0; i < t.templateCount; ++i) {
            const TemplateNote& tn = t.templ[i];
            if (tn.follows) {
                const int rel = tn.key - t.templateKey;
                const int degree = ((rel % 12) + 12) % 12;
                out[count++] = {uint8_t(fold(tn.key + lowest - t.templateKey + snap(degree, mask))), keys[0].velocity, false};
            } else out[count++] = {tn.key, keys[0].velocity, true};
        }
    }
    for (int i = t.keys == Keys::TemplateUpper ? 1 : 0; i < n; ++i) out[count++] = {keys[i].key, keys[i].velocity, false};
    return count;
}

void Player::sendNote(const Track& t, uint32_t frame, int channel, int key, int velocity7, uint32_t gate, double clockTick) {
    if (!audible_) return;
    const int slot = settings_.slot;
    const int vRate = clampi(settings_.velocityRate + settings_.slotVelocityOffset[slot], 0, 200);
    const int gRate = clampi(settings_.gateRate + settings_.slotGateOffset[slot], 0, 200);
    int v = int(velocity16(velocity7)) * vRate / 100;
    for (int i = 0; i < t.randomizeCount; ++i) {
        const Randomize& r = t.randomize[i];
        const int vlo = r.velLow < 2 ? 0 : velocity16(r.velLow), vhi = r.velHigh < 2 ? 0 : velocity16(r.velHigh);
        if (key < r.keyLow || key > r.keyHigh || v < vlo || v > vhi) continue;
        if (r.keyMax >= r.keyMin) key = fold(key + r.keyMin + int(nextRandom() % uint32_t(r.keyMax - r.keyMin + 1)));
        auto spread = [](int d) { return std::abs(d) <= 1 ? 0 : (d < 0 ? -1 : 1) * int(velocity16(std::abs(d))); };
        const int dlo = spread(r.velMin), dhi = spread(r.velMax);
        if (dhi >= dlo) v += dlo + int(nextRandom() % uint32_t(dhi - dlo + 1));
        break;
    }
    v = clampi(v, 1, 65535);
    double g = double(gate) * gRate / 100.0;
    g = std::min(std::max(g, 1.0), double((1u << 28) - 1));
    noteOn(frame, channel, key, uint16_t(v), clockTick + g, -1, false);
}

void Player::playEvent(int ti, const Event& e, double clockTick, uint32_t frame) {
    const Track& t = phrase_->tracks[ti];
    const int n = heldCount_;
    if (n < t.minHeld || (t.maxHeld < kAnyCount && n > t.maxHeld)) return;
    const int channel = t.channel ? t.channel - 1 : lastChannel_;
    const bool chordOk = chordType_ < 35 && (t.rootMask >> chordRoot_ & 1) && (t.typeMask >> chordType_ & 1);
    if (e.type != EventType::Note) {
        if (!audible_ || !chordOk) return;
        if (e.type == EventType::Control) emit({frame, Output::Control, uint8_t(channel), e.key, e.velocity, 0, 0, -1, -1});
        else if (e.type == EventType::Bend) emit({frame, Output::Bend, uint8_t(channel), 0, 0, 0, e.bend, -1, -1});
        else emit({frame, Output::Pressure, uint8_t(channel), 0, e.velocity, 0, 0, -1, -1});
        return;
    }
    if (settings_.keyMode == KeyMode::Direct) return;
    if (t.select == Select::Style) {
        if (chordOk) playStyle(t, e, channel, clockTick, frame);
        return;
    }
    const bool thru = settings_.velocityMode == VelocityMode::Thru;
    const int shift = 12 * settings_.outputOctave;
    auto out = [&](const Entry& k, int octave, int climb) {
        int key = k.key + octave;
        if (k.absolute) key = settings_.fixedSdBd ? sdbd(key) : key;
        else key += climb + shift;
        sendNote(t, frame, channel, fold(key), thru ? k.velocity : e.velocity, e.gate, clockTick);
    };
    if (e.source == Source::Key) { out({e.key, uint8_t(e.velocity), false}, 0, octaveStep_); return; }
    if (e.source == Source::Relative) {
        int lowest = 127;
        for (int i = 0; i < n; ++i) lowest = std::min<int>(lowest, held_[i].key);
        if (!n) return;
        out({uint8_t(fold(e.key + lowest - t.root)), held_[0].velocity, false}, 0, octaveStep_);
        return;
    }
    Entry list[kMaxList];
    Entry first{};
    const int m = buildList(t, list, first);
    const int pos = e.position, oct = 12 * e.octave;
    if (!m) {
        if (e.source == Source::Fallback) out({e.key, e.velocity, false}, 0, octaveStep_);
        return;
    }
    const int extra = pos >= t.lift ? 12 : 0;
    switch (t.select) {
        case Select::Index:
            if (pos < m) out(list[pos], oct, octaveStep_);
            else if (e.source == Source::Fallback) out({e.key, e.velocity, false}, 0, octaveStep_);
            break;
        case Select::IndexLast: {
            const int p = std::min(pos, m - 1);
            out(list[p], p == 0 && e.octave == 0 ? extra : oct, octaveStep_);
            break;
        }
        case Select::IndexFirst:
            if (pos < m) out(list[pos], oct, octaveStep_);
            else out(first, e.octave == 0 ? extra : oct, octaveStep_);
            break;
        case Select::Bounce:
        case Select::Bounce2:
        case Select::Cycle: {
            int p = 0;
            if (t.select == Select::Cycle) p = pos % m;
            else if (t.select == Select::Bounce) {
                if (m > 1) { p = pos % (2 * m - 2); if (p >= m) p = 2 * m - 2 - p; }
            } else { p = pos % (2 * m); if (p >= m) p = 2 * m - 1 - p; }
            out(list[p], p == 0 && e.octave == 0 ? extra : oct, octaveStep_);
            break;
        }
        default: {
            // Walkers: positions are ignored; they step through the list over their octave span.
            const int r = 12 * settings_.octaveRange, limit = 12 * (t.octaves - 1);
            // No ceiling on the span beyond the parameter ranges (notes still fold into 0..127).
            int lower = 0, upper = limit + r;
            if (r < 0) { lower = r; upper = limit; }
            Walk& w = walk_[ti];
            if (!w.started) { w = Walk{}; w.octave = lower; w.started = true; }
            w.octave = clampi(w.octave, lower, std::max(lower, upper));
            int i = 0;
            switch (t.select) {
                case Select::Up:
                case Select::Down:
                    if (w.step >= m) { w.octave = w.octave < upper ? w.octave + 12 : lower; w.step = 0; }
                    i = t.select == Select::Up ? w.step : m - 1 - w.step;
                    ++w.step;
                    out(list[i], oct + (t.select == Select::Up ? w.octave : -w.octave), 0);
                    return;
                case Select::Random: {
                    const int span = (upper - lower) / 12 + 1;
                    w.octave = lower + 12 * int(nextRandom() % uint32_t(std::max(1, span)));
                    i = int(nextRandom() % uint32_t(m));
                    out(list[i], oct + w.octave, 0);
                    return;
                }
                default: {
                    // Up-down: w.step is the index about to play; ends are played once (UpDown) or twice.
                    const bool twice = t.select == Select::UpDown2;
                    i = std::min(w.step, m - 1);
                    out(list[i], oct + w.octave, 0);
                    if (!w.down) {
                        if (w.step + 1 < m) ++w.step;
                        else if (w.octave < upper) { w.octave += 12; w.step = 0; }
                        else { w.down = true; w.step = twice ? m - 1 : std::max(0, m - 2); if (m == 1) w.octave = std::max(lower, w.octave - 12); }
                    } else {
                        if (w.step > 0 && m > 1) --w.step;
                        else if (w.octave > lower) { w.octave -= 12; w.step = m - 1; }
                        else { w.down = false; w.step = twice ? 0 : std::min(1, m - 1); if (m == 1) w.octave = std::min(upper, w.octave + 12); }
                    }
                    if (m == 1) w.step = 0;
                    return;
                }
            }
        }
    }
}

// Style tracks: the event's key is a source pitch written for (styleRoot, styleType); move it to the
// held chord. An approximation of root transposition plus chord-tone mapping (plan R25).
void Player::playStyle(const Track& t, const Event& e, int channel, double clockTick, uint32_t frame) {
    int lo = t.styleLow, hi = t.styleHigh;
    if (hi < lo + 11) { lo = hi - 11; if (lo < 0) { lo = 0; hi = 11; } }
    const StyleRule& rule = t.rules[e.key < lo ? 0 : e.key > hi ? 2 : 1];
    const int target = chordType_ < 34 ? chordType_ : 30;
    const uint16_t from = t.styleType < 34 ? kChordShapes[t.styleType] : kChordShapes[2];
    const uint16_t to = kChordShapes[target];
    const int root = rule.bass ? chordBass_ : chordRoot_;
    int key = e.key;
    if (rule.transpose == 2 && (key < 48 || key > 95)) return;
    const int degree = ((key - t.styleRoot) % 12 + 12) % 12;
    // Chord tones of the source chord move to the matching tones of the held chord.
    int delta = 0;
    if (degree == thirdOf(from) && thirdOf(to) >= 0) delta = thirdOf(to) - degree;
    else if (degree == fifthOf(from) && fifthOf(to) >= 0) delta = fifthOf(to) - degree;
    else if (degree == seventhOf(from)) delta = seventhOf(to) >= 0 ? seventhOf(to) - degree : (to & 0x200 ? 9 - degree : 0);
    if (rule.transpose == 1) {
        // Root fixed: keep the register, move to the nearest tone of the held chord.
        int best = 99;
        for (int d = -6; d <= 6; ++d) {
            const int pc = ((key + d - root) % 12 + 12) % 12;
            if ((to >> pc & 1) && std::abs(d) < std::abs(best)) best = d;
        }
        key += best == 99 ? 0 : best;
    } else {
        int s = ((root - t.styleRoot) % 12 + 12) % 12;
        if (s > rule.highKey) s -= 12;
        key += s + delta;
    }
    const int rlo = rule.low & 0x7F, rhi = rule.high & 0x7F;
    if (rlo <= rhi && (key < rlo || key > rhi)) {
        if ((rule.low | rule.high) & 0x80) return;
        while (key < rlo) key += 12;
        while (key > rhi) key -= 12;
    }
    if (!(rule.transpose == 1 && rule.table == 0)) key += octaveStep_ + 12 * settings_.outputOctave;
    int vel = e.velocity;
    if (settings_.velocityMode == VelocityMode::Thru && heldCount_) {
        Entry list[kMaxList];
        Entry first{};
        Track heldOnly = t;
        heldOnly.keys = Keys::Held;
        if (buildList(heldOnly, list, first)) vel = list[0].velocity;
    }
    sendNote(t, frame, channel, fold(key), vel, e.gate, clockTick);
}

void Player::advanceTo(uint32_t frame) {
    if (frame > blockFrames_) frame = blockFrames_;
    while (!overflow_) {
        // The earliest pending moment: a note end, a delayed start, an accent start, the next event of
        // either pass, or a pass's end. Note ends come first at equal ticks.
        double next = kNever;
        int kind = -1, which = -1;
        for (int i = 0; i < kMaxSounding; ++i)
            if (sounding_[i].used && sounding_[i].offTick < next) { next = sounding_[i].offTick; kind = 0; which = i; }
        if (startAt_ >= 0 && startAt_ < next) { next = startAt_; kind = 1; }
        const bool playing = running_ && startAt_ < 0 && phrase_;
        if (playing && accent_ == 1 && accentAt_ < next) { next = accentAt_; kind = 2; }
        auto nextIn = [&](const Sched* s, int count, double pos, double base, int& idx) {
            int lo = 0, hi = count;
            while (lo < hi) { const int mid = (lo + hi) / 2; if (s[mid].tick < pos) lo = mid + 1; else hi = mid; }
            idx = lo < count && s[lo].tick < phrase_->length ? lo : -1;
            return idx >= 0 ? base + s[lo].tick : base + phrase_->length;
        };
        int idx = -1;
        if (playing) {
            // The accent pass first, so its end at a tick lets the normal events at that tick play.
            if (accent_ == 2) {
                int aidx = -1;
                const double ta = nextIn(accentSched_, accentCount_, accentPos_, accentAt_, aidx);
                if (ta < next) { next = ta; kind = aidx >= 0 ? 5 : 6; which = aidx; }
            }
            const double t = nextIn(sched_, schedCount_, nextPos_, origin_, idx);
            if (t < next) { next = t; kind = idx >= 0 ? 3 : 4; which = idx; }
        }
        if (next == kNever) break;
        // Events belong to the first frame whose clock reaches them; that frame decides the block.
        const uint32_t f = frameAt(next);
        if (f >= frame) break;
        switch (kind) {
            case 0: noteOffAt(f, which); break;
            case 1: origin_ = startAt_; startAt_ = -1; nextPos_ = 0; if (accent_ == 1) accentAt_ = std::max(accentAt_, origin_); break;
            case 2: accent_ = 2; accentPos_ = 0; break;
            case 3:
            case 5: {
                const Sched* s = kind == 3 ? sched_ : accentSched_;
                const int count = kind == 3 ? schedCount_ : accentCount_;
                const double tick = s[which].tick;
                // Accent tracks play on top of the normal ones (measured), from the accent's own start.
                for (int i = which; i < count && s[i].tick == tick; ++i)
                    playEvent(s[i].track, phrase_->events[s[i].event], next, f);
                (kind == 3 ? nextPos_ : accentPos_) = std::nextafter(tick, kNever);
                break;
            }
            case 6: accent_ = 0; break;  // the accent pass is over
            case 4: {
                // A pass ends: switch a pending slot at the bar line, climb the octave range, loop or stop.
                origin_ += phrase_->length;
                nextPos_ = 0;
                if (pendingSlot_ >= 0) {
                    settings_.slot = pendingSlot_;
                    pendingSlot_ = -1;
                    phrase_ = slots_[settings_.slot];
                    rebuildSchedule();
                    for (auto& w : walk_) w = Walk{};
                    if (!phrase_) { stopArp(f); break; }
                }
                const int r = 12 * settings_.octaveRange;
                if (r > 0) octaveStep_ = octaveStep_ < r ? octaveStep_ + 12 : 0;
                else if (r < 0) octaveStep_ = octaveStep_ > r ? octaveStep_ - 12 : 0;
                else octaveStep_ = 0;
                if (!settings_.loop) running_ = audible_ = false;
                break;
            }
            default: break;
        }
    }
    cursorFrame_ = frame;
}

}  // namespace arp
