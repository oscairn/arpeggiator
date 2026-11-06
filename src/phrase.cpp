// SPDX-License-Identifier: MIT
#include "phrase.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace arp {
namespace {

const char* const kSelectNames[kSelectCount] = {"index", "index-last", "index-first", "bounce", "bounce2", "cycle",
                                                "up",    "down",       "updown",      "updown2", "random", "style"};
const char* const kKeysNames[3] = {"held", "template+held", "template+upper"};
const char* const kRuleNames[3] = {"low", "mid", "high"};

struct Cursor {
    const char* p;
    const char* end;
    int line = 1;
    bool atEnd() const { return p >= end; }
};

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

// Next whitespace-separated word on the current line; a quoted string keeps its spaces and may be empty.
bool word(Cursor& c, const char*& w, size_t& n) {
    while (!c.atEnd() && isSpace(*c.p)) ++c.p;
    if (c.atEnd() || *c.p == '\n' || *c.p == '#') return false;
    if (*c.p == '"') {
        w = ++c.p;
        while (!c.atEnd() && *c.p != '"' && *c.p != '\n') ++c.p;
        n = size_t(c.p - w);
        if (!c.atEnd() && *c.p == '"') ++c.p;
        return true;
    }
    w = c.p;
    while (!c.atEnd() && !isSpace(*c.p) && *c.p != '\n' && *c.p != '#') ++c.p;
    n = size_t(c.p - w);
    return true;
}

void nextLine(Cursor& c) {
    while (!c.atEnd() && *c.p != '\n') ++c.p;
    if (!c.atEnd()) { ++c.p; ++c.line; }
}

bool eq(const char* w, size_t n, const char* s) { return std::strlen(s) == n && !std::memcmp(w, s, n); }

// A signed decimal (or 0x hex) at w[0..n). With `used`, trailing text is allowed and its offset returned.
bool number(const char* w, size_t n, long long& v, size_t* used = nullptr) {
    size_t i = 0;
    bool neg = false;
    if (i < n && (w[i] == '-' || w[i] == '+')) neg = w[i++] == '-';
    long long x = 0;
    if (i + 2 < n + 1 && i + 1 < n && w[i] == '0' && w[i + 1] == 'x') {
        i += 2;
        const size_t start = i;
        for (; i < n; ++i) {
            const char ch = w[i];
            const int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
            if (d < 0) break;
            if (i - start >= 15) return false;
            x = x * 16 + d;
        }
        if (i == start) return false;
    } else {
        if (i >= n || w[i] < '0' || w[i] > '9') return false;
        while (i < n && w[i] >= '0' && w[i] <= '9') {
            x = x * 10 + (w[i++] - '0');
            if (x > 1000000000) return false;
        }
    }
    v = neg ? -x : x;
    if (used) *used = i;
    else if (i != n) return false;
    return true;
}

bool wordNumber(Cursor& c, long long& v, long long lo, long long hi) {
    const char* w; size_t n;
    return word(c, w, n) && number(w, n, v) && v >= lo && v <= hi;
}

bool source(const char* w, size_t n, Event& e) {
    long long v; size_t used;
    if (n < 2) return false;
    if (w[0] == 'k' || w[0] == 'r') {
        if (!number(w + 1, n - 1, v) || v < 0 || v > 127) return false;
        e.source = w[0] == 'k' ? Source::Key : Source::Relative;
        e.key = uint8_t(v);
        return true;
    }
    if (w[0] != 'p' || w[1] == '-' || w[1] == '+' || !number(w + 1, n - 1, v, &used) || v < 0 || v > 15) return false;
    e.source = Source::Position;
    e.position = uint8_t(v);
    size_t i = 1 + used;
    if (i < n && (w[i] == '+' || w[i] == '-')) {
        size_t u2;
        if (!number(w + i, n - i, v, &u2) || v < -10 || v > 10) return false;
        e.octave = int8_t(v);
        i += u2;
    }
    if (i == n) return true;
    if (w[i] != '/' || i + 2 > n || w[i + 1] != 'k' || !number(w + i + 2, n - i - 2, v) || v < 0 || v > 127) return false;
    e.source = Source::Fallback;
    e.key = uint8_t(v);
    return true;
}

}  // namespace

bool parsePhrase(const char* text, size_t size, Phrase& out, char* error, size_t errorSize) {
    Cursor c{text, text + size};
    auto fail = [&](const char* what) {
        if (error && errorSize) std::snprintf(error, errorSize, "line %d: %s", c.line, what);
        return false;
    };
    out.trackCount = 0;
    out.eventCount = 0;
    out.sfxCount = 0;
    out.name[0] = out.author[0] = out.tags[0] = 0;
    out.ppq = 480; out.length = 1920; out.meterNum = 4; out.meterDen = 4;
    out.hasAccent = false;
    const char* w; size_t n; long long v;
    while (!c.atEnd() && !word(c, w, n)) nextLine(c);
    if (c.atEnd() || !eq(w, n, "oscairn-phrase")) return fail("expected 'oscairn-phrase 1'");
    if (!wordNumber(c, v, 1, 1000)) return fail("missing format version");
    if (v != 1) return fail("unsupported format version");
    nextLine(c);
    Track* track = nullptr;
    uint32_t lastTick = 0;
    while (!c.atEnd()) {
        if (!word(c, w, n)) { nextLine(c); continue; }
        if (!track) {
            if (eq(w, n, "name") || eq(w, n, "author") || eq(w, n, "tags")) {
                char* dst = w[0] == 'n' ? out.name : w[0] == 'a' ? out.author : out.tags;
                const size_t cap = w[0] == 't' ? size_t(kTagsSize) : size_t(kNameSize);
                if (!word(c, w, n)) return fail("name, author and tags need a value");
                n = std::min(n, cap - 1);
                std::memcpy(dst, w, n);
                dst[n] = 0;
            } else if (eq(w, n, "ppq")) {
                if (!wordNumber(c, v, 24, 3840)) return fail("ppq must be 24..3840");
                out.ppq = uint32_t(v);
            } else if (eq(w, n, "length")) {
                if (!wordNumber(c, v, 1, 100000000)) return fail("length must be 1..100000000 ticks");
                out.length = uint32_t(v);
            } else if (eq(w, n, "meter")) {
                long long d;
                if (!wordNumber(c, v, 1, 64) || !wordNumber(c, d, 1, 64)) return fail("meter needs two numbers 1..64");
                out.meterNum = uint8_t(v); out.meterDen = uint8_t(d);
            } else if (eq(w, n, "sfx")) {
                long long a[5];
                if (out.sfxCount == kMaxSfx) return fail("too many sfx lines");
                for (int i = 0; i < 4; ++i) if (!wordNumber(c, a[i], 0, 127)) return fail("sfx needs key low/high, velocity low/high, gate");
                if (!wordNumber(c, a[4], 1, 100000000)) return fail("sfx gate must be positive");
                out.sfx[out.sfxCount++] = {uint8_t(a[0]), uint8_t(a[1]), uint8_t(a[2]), uint8_t(a[3]), uint32_t(a[4])};
            } else if (eq(w, n, "track")) {
                if (out.trackCount == kMaxTracks) return fail("too many tracks");
                track = &out.tracks[out.trackCount++];
                *track = Track{};
                track->first = out.eventCount;
                lastTick = 0;
            } else return fail("unknown header keyword");
            if (word(c, w, n)) return fail("unexpected text at end of line");
            nextLine(c);
            continue;
        }
        if (eq(w, n, "end")) {
            track->count = out.eventCount - track->first;
            track = nullptr;
        } else if (eq(w, n, "select")) {
            if (!word(c, w, n)) return fail("select needs a value");
            int found = -1;
            for (int i = 0; i < kSelectCount; ++i) if (eq(w, n, kSelectNames[i])) found = i;
            if (found < 0) return fail("unknown select");
            track->select = Select(found);
        } else if (eq(w, n, "keys")) {
            if (!word(c, w, n)) return fail("keys needs a value");
            int found = -1;
            for (int i = 0; i < 3; ++i) if (eq(w, n, kKeysNames[i])) found = i;
            if (found < 0) return fail("keys must be held, template+held or template+upper");
            track->keys = Keys(found);
        } else if (eq(w, n, "accent")) {
            track->accent = true;
            out.hasAccent = true;
        } else if (eq(w, n, "held")) {
            if (!wordNumber(c, v, 1, 127)) return fail("held needs a minimum 1..127");
            track->minHeld = uint8_t(v);
            if (!word(c, w, n)) return fail("held needs a maximum");
            if (eq(w, n, "any")) track->maxHeld = kAnyCount;
            else if (!number(w, n, v) || v < 1 || v > 127) return fail("held maximum must be 1..127 or any");
            else track->maxHeld = uint8_t(v);
        } else if (eq(w, n, "octaves")) {
            if (!wordNumber(c, v, 1, 4)) return fail("octaves must be 1..4");
            track->octaves = uint8_t(v);
        } else if (eq(w, n, "lift")) {
            if (!wordNumber(c, v, 0, 15)) return fail("lift must be 0..15");
            track->lift = uint8_t(v);
        } else if (eq(w, n, "root")) {
            if (!wordNumber(c, v, 0, 127)) return fail("root must be 0..127");
            track->root = uint8_t(v);
        } else if (eq(w, n, "channel")) {
            if (!wordNumber(c, v, 0, 16)) return fail("channel must be 0..16");
            track->channel = uint8_t(v);
        } else if (eq(w, n, "template")) {
            if (!wordNumber(c, v, 0, 127)) return fail("template needs its key first");
            track->templateKey = uint8_t(v);
            track->templateCount = 0;
            while (word(c, w, n)) {
                if (track->templateCount == kMaxTemplate) return fail("too many template notes");
                if (!n || (w[0] != 'c' && w[0] != 'a') || !number(w + 1, n - 1, v) || v < 0 || v > 127)
                    return fail("template notes are c<key> (follows the chord) or a<key> (absolute)");
                track->templ[track->templateCount++] = {uint8_t(v), w[0] == 'c'};
            }
        } else if (eq(w, n, "randomize")) {
            long long a[8];
            if (track->randomizeCount == kMaxRandomize) return fail("too many randomize lines");
            for (int i = 0; i < 4; ++i) if (!wordNumber(c, a[i], 0, 127)) return fail("randomize needs key low/high, velocity low/high");
            for (int i = 4; i < 8; ++i) if (!wordNumber(c, a[i], -64, 63)) return fail("randomize needs key and velocity spreads -64..63");
            track->randomize[track->randomizeCount++] = {uint8_t(a[0]), uint8_t(a[1]), uint8_t(a[2]), uint8_t(a[3]),
                                                         int8_t(a[4]), int8_t(a[5]), int8_t(a[6]), int8_t(a[7])};
        } else if (eq(w, n, "chords")) {
            long long t;
            if (!wordNumber(c, v, 0, 0xFFF) || !wordNumber(c, t, 0, 0x7FFFFFFFFll)) return fail("chords needs a root mask and a type mask");
            track->rootMask = uint16_t(v); track->typeMask = uint64_t(t);
        } else if (eq(w, n, "style")) {
            long long t;
            if (!wordNumber(c, v, 0, 11) || !wordNumber(c, t, 0, 34)) return fail("style needs a root 0..11 and a chord type 0..34");
            track->styleRoot = uint8_t(v); track->styleType = uint8_t(t);
        } else if (eq(w, n, "style-range")) {
            long long hi;
            if (!wordNumber(c, v, 0, 127) || !wordNumber(c, hi, 0, 127)) return fail("style-range needs low and high");
            track->styleLow = uint8_t(v); track->styleHigh = uint8_t(hi);
        } else if (eq(w, n, "rule")) {
            if (!word(c, w, n)) return fail("rule needs low, mid or high");
            int r = -1;
            for (int i = 0; i < 3; ++i) if (eq(w, n, kRuleNames[i])) r = i;
            if (r < 0) return fail("rule needs low, mid or high");
            long long a[7];
            const long long hi[7] = {2, 10, 1, 11, 255, 255, 4};
            for (int i = 0; i < 7; ++i) if (!wordNumber(c, a[i], 0, hi[i])) return fail("rule needs transpose table bass high-key low high retrigger");
            track->rules[r] = {uint8_t(a[0]), uint8_t(a[1]), a[2] != 0, uint8_t(a[3]), uint8_t(a[4]), uint8_t(a[5]), uint8_t(a[6])};
        } else if (eq(w, n, "note") || eq(w, n, "cc") || eq(w, n, "bend") || eq(w, n, "pressure")) {
            if (out.eventCount == kMaxEvents) return fail("too many events");
            Event e{};
            const char kind = w[0];
            if (!wordNumber(c, v, 0, 100000000)) return fail("event needs a tick");
            e.tick = uint32_t(v);
            if (e.tick < lastTick) return fail("events must be in tick order");
            lastTick = e.tick;
            if (kind == 'n') {
                e.type = EventType::Note;
                if (!word(c, w, n) || !source(w, n, e)) return fail("bad note source (k60, r60, p2, p2+1, p2/k64)");
                if (!word(c, w, n) || !n || w[0] != 'v' || !number(w + 1, n - 1, v) || v < 1 || v > 127) return fail("note needs v1..v127");
                e.velocity = uint8_t(v);
                if (!word(c, w, n) || !n || w[0] != 'g' || !number(w + 1, n - 1, v) || v < 1 || v > 100000000) return fail("note needs g<ticks>");
                e.gate = uint32_t(v);
            } else if (kind == 'c') {
                long long val;
                if (!wordNumber(c, v, 0, 127) || !wordNumber(c, val, 0, 127)) return fail("cc needs number and value");
                e.type = EventType::Control; e.key = uint8_t(v); e.velocity = uint8_t(val);
            } else if (kind == 'b') {
                if (!wordNumber(c, v, -8192, 8191)) return fail("bend must be -8192..8191");
                e.type = EventType::Bend; e.bend = int16_t(v);
            } else {
                if (!wordNumber(c, v, 0, 127)) return fail("pressure must be 0..127");
                e.type = EventType::Pressure; e.velocity = uint8_t(v);
            }
            out.events[out.eventCount++] = e;
        } else return fail("unknown track keyword");
        if (word(c, w, n)) return fail("unexpected text at end of line");
        nextLine(c);
    }
    if (track) return fail("track without 'end'");
    if (!out.trackCount) return fail("no tracks");
    return true;
}

size_t writePhrase(const Phrase& ph, char* out, size_t size) {
    size_t used = 0;
    auto put = [&](const char* fmt, auto... args) {
        char buf[200];
        const int k = std::snprintf(buf, sizeof buf, fmt, args...);
        if (k > 0) {
            if (used + size_t(k) < size) std::memcpy(out + used, buf, size_t(k));
            used += size_t(k);
        }
    };
    auto clean = [](const char* in, size_t cap, char* out) {
        size_t j = 0;
        for (size_t i = 0; i < cap && in[i] && j + 1 < cap; ++i)
            if (in[i] != '"' && in[i] != '\n' && in[i] != '\r') out[j++] = in[i];
        out[j] = 0;
    };
    char name[kNameSize], author[kNameSize], tags[kTagsSize];
    clean(ph.name, kNameSize, name);
    clean(ph.author, kNameSize, author);
    clean(ph.tags, kTagsSize, tags);
    put("oscairn-phrase 1\nname \"%s\"\n", name);
    if (author[0]) put("author \"%s\"\n", author);
    if (tags[0]) put("tags \"%s\"\n", tags);
    put("ppq %u\nlength %u\nmeter %d %d\n", ph.ppq, ph.length, ph.meterNum, ph.meterDen);
    for (int i = 0; i < ph.sfxCount; ++i)
        put("sfx %d %d %d %d %u\n", ph.sfx[i].keyLow, ph.sfx[i].keyHigh, ph.sfx[i].velLow, ph.sfx[i].velHigh, ph.sfx[i].gate);
    const Track def{};
    for (int t = 0; t < ph.trackCount; ++t) {
        const Track& tr = ph.tracks[t];
        put("track\n  select %s\n", kSelectNames[int(tr.select)]);
        if (tr.keys != Keys::Held) put("  keys %s\n", kKeysNames[int(tr.keys)]);
        if (tr.accent) put("  %s\n", "accent");
        if (tr.minHeld != 1 || tr.maxHeld != kAnyCount) {
            if (tr.maxHeld == kAnyCount) put("  held %d any\n", tr.minHeld);
            else put("  held %d %d\n", tr.minHeld, tr.maxHeld);
        }
        if (tr.octaves != 1) put("  octaves %d\n", tr.octaves);
        if (tr.lift != 255) put("  lift %d\n", tr.lift);
        if (tr.root != 60) put("  root %d\n", tr.root);
        if (tr.channel) put("  channel %d\n", tr.channel);
        if (tr.templateCount) {
            put("  template %d", tr.templateKey);
            for (int i = 0; i < tr.templateCount; ++i) put(" %c%d", tr.templ[i].follows ? 'c' : 'a', tr.templ[i].key);
            put("%s", "\n");
        }
        for (int i = 0; i < tr.randomizeCount; ++i) {
            const Randomize& r = tr.randomize[i];
            put("  randomize %d %d %d %d %d %d %d %d\n", r.keyLow, r.keyHigh, r.velLow, r.velHigh, r.keyMin, r.keyMax, r.velMin, r.velMax);
        }
        if (tr.rootMask != def.rootMask || tr.typeMask != def.typeMask)
            put("  chords 0x%03x 0x%09llx\n", tr.rootMask, (unsigned long long)tr.typeMask);
        if (tr.select == Select::Style) {
            put("  style %d %d\n", tr.styleRoot, tr.styleType);
            if (tr.styleLow || tr.styleHigh != 127) put("  style-range %d %d\n", tr.styleLow, tr.styleHigh);
            for (int r = 0; r < 3; ++r) {
                const StyleRule& s = tr.rules[r];
                put("  rule %s %d %d %d %d %d %d %d\n", kRuleNames[r], s.transpose, s.table, s.bass ? 1 : 0, s.highKey, s.low, s.high, s.retrigger);
            }
        }
        for (int i = tr.first; i < tr.first + tr.count; ++i) {
            const Event& e = ph.events[i];
            switch (e.type) {
                case EventType::Note:
                    switch (e.source) {
                        case Source::Key: put("  note %u k%d", e.tick, e.key); break;
                        case Source::Relative: put("  note %u r%d", e.tick, e.key); break;
                        default:
                            put("  note %u p%d", e.tick, e.position);
                            if (e.octave) put("%+d", e.octave);
                            if (e.source == Source::Fallback) put("/k%d", e.key);
                    }
                    put(" v%d g%u\n", e.velocity, e.gate);
                    break;
                case EventType::Control: put("  cc %u %d %d\n", e.tick, e.key, e.velocity); break;
                case EventType::Bend: put("  bend %u %d\n", e.tick, e.bend); break;
                case EventType::Pressure: put("  pressure %u %d\n", e.tick, e.velocity); break;
            }
        }
        put("%s", "end\n");
    }
    if (used < size) out[used] = 0;
    return used;
}

}  // namespace arp
