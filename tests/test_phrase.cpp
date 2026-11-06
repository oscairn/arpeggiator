// SPDX-License-Identifier: MIT
#include <cstring>
#include <memory>
#include <string>

#include "check.h"
#include "phrase.h"

using namespace arp;

static bool parse(const std::string& s, Phrase& p, std::string* err = nullptr) {
    char e[128] = {};
    const bool ok = parsePhrase(s.data(), s.size(), p, e, sizeof e);
    if (err) *err = e;
    return ok;
}

TEST(phrase_parses_all_event_kinds) {
    auto p = std::make_unique<Phrase>();
    const std::string text =
        "# comment\noscairn-phrase 1\nname \"Two words\"\nppq 480\nlength 960\nmeter 2 4\n"
        "sfx 120 127 30 50 100\n"
        "track\n  select index-last\n  keys template+upper\n  held 2 3\n  template 48 c55 a36\n  accent\n"
        "  randomize 0 127 2 127 -1 1 -3 3\n  note 0 p0 v100 g60\n  note 120 p1+1 v90 g60\n"
        "  note 240 p2-1/k64 v80 g30\n  note 360 k60 v1 g1\n  note 480 r62 v127 g480\n  cc 480 74 10\n"
        "  bend 600 -8192\n  pressure 700 5\nend\n";
    CHECK(parse(text, *p));
    CHECK(!std::strcmp(p->name, "Two words"));
    CHECK_EQ(p->length, 960u);
    CHECK_EQ(p->meterNum, 2);
    CHECK_EQ(p->trackCount, 1);
    CHECK(p->tracks[0].select == Select::IndexLast);
    CHECK(p->tracks[0].keys == Keys::TemplateUpper);
    CHECK(p->tracks[0].accent);
    CHECK_EQ(p->tracks[0].templateCount, 2);
    CHECK(p->tracks[0].templ[0].follows && !p->tracks[0].templ[1].follows);
    CHECK_EQ(p->tracks[0].templ[1].key, 36);
    CHECK_EQ(p->tracks[0].randomize[0].velMin, -3);
    CHECK_EQ(p->sfxCount, 1);
    CHECK_EQ(p->sfx[0].gate, 100u);
    CHECK_EQ(p->tracks[0].minHeld, 2);
    CHECK_EQ(p->tracks[0].count, 8);
    CHECK(p->events[1].source == Source::Position && p->events[1].position == 1 && p->events[1].octave == 1);
    CHECK(p->events[2].source == Source::Fallback && p->events[2].octave == -1 && p->events[2].key == 64);
    CHECK(p->events[4].source == Source::Relative && p->events[4].key == 62);
    CHECK(p->events[6].type == EventType::Bend && p->events[6].bend == -8192);
    CHECK(p->hasAccent);
}

TEST(phrase_round_trips_through_text) {
    auto a = std::make_unique<Phrase>(), b = std::make_unique<Phrase>();
    const std::string text =
        "oscairn-phrase 1\nname \"x\"\nauthor \"Jacob Schooley\"\ntags \"bass, 8 steps\"\nlength 1920\ntrack\n  select up\n  octaves 2\n  channel 10\n  held 4 any\n"
        "  note 0 p0 v100 g60\n  cc 5 1 2\nend\ntrack\n  select style\n  chords 0x0f0 0x3ffffffff\n  style 7 8\n"
        "  rule low 1 3 1 5 0 255 4\n  note 10 k36 v100 g60\nend\n";
    CHECK(parse(text, *a));
    std::string out(65536, '\0');
    const size_t n = writePhrase(*a, out.data(), out.size());
    out.resize(n);
    CHECK(parse(out, *b));
    std::string again(65536, '\0');
    again.resize(writePhrase(*b, again.data(), again.size()));
    CHECK(out == again);
    CHECK(!std::strcmp(b->author, "Jacob Schooley"));
    CHECK(!std::strcmp(b->tags, "bass, 8 steps"));
    CHECK_EQ(b->tracks[0].octaves, 2);
    CHECK_EQ(b->tracks[0].channel, 10);
    CHECK_EQ(b->tracks[1].first, 2);
    CHECK_EQ(b->tracks[0].minHeld, 4);
    CHECK_EQ(b->tracks[0].maxHeld, kAnyCount);
    CHECK_EQ(b->tracks[1].rootMask, 0x0f0);
    CHECK(b->tracks[1].typeMask == 0x3ffffffffull);
    CHECK(b->tracks[1].rules[0].bass && b->tracks[1].rules[0].high == 255 && b->tracks[1].rules[0].retrigger == 4);
    CHECK_EQ(b->tracks[1].styleType, 8);
}

TEST(phrase_rejects_bad_input) {
    auto p = std::make_unique<Phrase>();
    std::string err;
    CHECK(!parse("", *p));
    CHECK(!parse("oscairn-phrase 2\ntrack\nend\n", *p, &err));
    CHECK(err.find("version") != std::string::npos);
    CHECK(!parse("oscairn-phrase 1\ntrack\n  note 10 p0 v1 g1\n  note 5 p0 v1 g1\nend\n", *p, &err));
    CHECK(err.find("line 4") != std::string::npos);
    CHECK(!parse("oscairn-phrase 1\ntrack\n  note 0 p0 v0 g1\nend\n", *p));
    CHECK(!parse("oscairn-phrase 1\ntrack\n  note 0 q0 v1 g1\nend\n", *p));
    CHECK(!parse("oscairn-phrase 1\ntrack\n  note 0 p0 v1 g1\n", *p, &err));
    CHECK(!parse("oscairn-phrase 1\nname \"x\"\n", *p));
    CHECK(!parse("oscairn-phrase 1\ntrack\n  note 0 p0 v1 g1 loud\nend\n", *p));
    CHECK(!parse("oscairn-phrase 1\ntrack\n  note 0 p16 v1 g1\nend\n", *p));
    CHECK(!parse("oscairn-phrase 1\ntrack\n  template 60 x62\nend\n", *p));
}

TEST(phrase_rejects_too_many_events) {
    auto p = std::make_unique<Phrase>();
    std::string text = "oscairn-phrase 1\ntrack\n";
    for (int i = 0; i <= kMaxEvents; ++i) text += "  cc " + std::to_string(i) + " 1 1\n";
    text += "end\n";
    std::string err;
    CHECK(!parse(text, *p, &err));
    CHECK(err.find("too many events") != std::string::npos);
}

// Parses text from a heap buffer of exactly its size, so a read past the end is a sanitizer error.
static bool parseExact(const std::string& s, Phrase& p) {
    std::unique_ptr<char[]> buf(new char[s.size() ? s.size() : 1]);
    std::memcpy(buf.get(), s.data(), s.size());
    char e[128];
    return parsePhrase(buf.get(), s.size(), p, e, sizeof e);
}

TEST(phrase_parser_stays_inside_its_buffer) {
    auto p = std::make_unique<Phrase>();
    // An empty quoted word ("") where a letter-prefixed word is expected, at the very end of the text.
    CHECK(!parseExact("oscairn-phrase 1\ntrack\n  note 0 p0 \"", *p));
    CHECK(!parseExact("oscairn-phrase 1\ntrack\n  note 0 p0 \"\" g1\nend\n", *p));
    CHECK(!parseExact("oscairn-phrase 1\ntrack\n  note 0 p0 v1 \"", *p));
    CHECK(!parseExact("oscairn-phrase 1\ntrack\n  template 60 \"", *p));
    CHECK(!parseExact("oscairn-phrase 1\ntrack\n  note 0 \"", *p));
}

TEST(phrase_parser_survives_mutations_and_writes_a_fixed_point) {
    // Random edits of a phrase that uses every statement: parsing never reads outside the text, and
    // whatever parses writes to a text that parses to the same text again.
    const std::string base =
        "oscairn-phrase 1\nname \"Two words\"\nauthor \"J\"\ntags \"a, b\"\nppq 96\nlength 960\nmeter 2 4\nsfx 120 127 30 50 100\n"
        "track\n  select index-last\n  keys template+upper\n  held 2 3\n  template 48 c55 a36\n  accent\n  lift 1\n  root 50\n  channel 3\n"
        "  randomize 0 127 2 127 -1 1 -3 3\n  chords 0x0f0 0x3ffffffff\n  note 0 p0 v100 g60\n  note 120 p1+1 v90 g60\n"
        "  note 240 p2-1/k64 v80 g30\n  note 360 k60 v1 g1\n  note 480 r62 v127 g480\n  cc 480 74 10\n  bend 600 -8192\n  pressure 700 5\nend\n"
        "track\n  select style\n  style 7 8\n  style-range 40 90\n  rule low 1 3 1 5 0 255 4\n  note 10 k36 v100 g60\nend\n"
        "track\n  select up\n  octaves 3\n  note 0 p0 v100 g10\nend\n";
    const char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz \n\"#-+/x\t\r";
    uint32_t r = 0x9E3779B9u;
    auto rnd = [&](size_t n) { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return size_t(r % uint32_t(n)); };
    auto a = std::make_unique<Phrase>(), b = std::make_unique<Phrase>();
    std::string out(1 << 16, '\0'), again(1 << 16, '\0');
    int parsed = 0;
    for (int i = 0; i < 3000; ++i) {
        std::string s = base;
        for (int m = 0, n = 1 + int(rnd(3)); m < n; ++m) {
            const size_t at = rnd(s.size());
            switch (rnd(5)) {
                case 0: s[at] = alphabet[rnd(sizeof alphabet - 1)]; break;
                case 1: s.erase(at, 1 + rnd(3)); break;
                case 2: s.insert(at, 1, alphabet[rnd(sizeof alphabet - 1)]); break;
                case 3: if (s[at] >= '0' && s[at] <= '9') s[at] = char('0' + rnd(10)); break;  // another value
                default: { const size_t from = rnd(s.size()); s.insert(at, s.substr(from, rnd(40))); break; }
            }
        }
        if (!parseExact(s, *a)) continue;
        ++parsed;
        const size_t n = writePhrase(*a, out.data(), out.size());
        CHECK(n < out.size());
        if (!parseExact(std::string(out.data(), n), *b)) { ++g_failures; std::printf("  written phrase does not parse:\n%.*s\n", int(n), out.data()); continue; }
        const size_t n2 = writePhrase(*b, again.data(), again.size());
        if (n != n2 || std::memcmp(out.data(), again.data(), n)) { ++g_failures; std::printf("  not a fixed point:\n%.*s---\n%.*s\n", int(n), out.data(), int(n2), again.data()); }
    }
    CHECK(parsed > 300);
}
