// SPDX-License-Identifier: MIT
#include <cstring>
#include <memory>

#include "check.h"
#include "phrase.h"
#include "record.h"

using namespace arp;

TEST(recorder_captures_from_the_first_key_for_its_length) {
    auto r = std::make_unique<Recorder>();
    r->arm(1920);
    CHECK(!r->advance(5000));            // nothing played yet: the clock has not started
    r->keyOn(1000, 60, 90);              // starts the recording at tick 1000
    r->keyOn(1000, 64, 80);
    r->keyOff(1240, 60);
    r->keyOn(1960, 76, 70);
    CHECK(!r->advance(2900));
    CHECK(r->advance(2920));             // 1920 ticks after the first key; 64 and 76 still held
    CHECK(r->done() && !r->armed());
    CHECK_EQ(r->count(), 3);
    CHECK_EQ(r->notes()[0].gate, 240u);
    CHECK_EQ(r->notes()[1].gate, 1920u);
    CHECK_EQ(r->notes()[2].tick, 960u);
    CHECK_EQ(r->notes()[2].gate, 960u);
}

TEST(recorded_phrase_converts_like_the_import_modes) {
    const RecordedNote n[] = {{0, 240, 60, 90}, {0, 240, 64, 80}, {0, 240, 67, 80}, {480, 120, 76, 70}, {720, 120, 62, 60}};
    auto p = std::make_unique<Phrase>();
    char err[128];
    std::string chord = recordedPhrase(n, 5, 1920, RecordMode::Chord, "Recorded");
    CHECK(parsePhrase(chord.data(), chord.size(), *p, err, sizeof err));
    CHECK(chord.find("note 480 p1+1 v70 g120") != std::string::npos);
    CHECK(chord.find("note 720 r62 v60 g120") != std::string::npos);
    CHECK(chord.find("root 60") != std::string::npos);
    std::string fixed = recordedPhrase(n, 5, 1920, RecordMode::Fixed, "Recorded");
    CHECK(fixed.find("note 480 k76") != std::string::npos);
    std::string rel = recordedPhrase(n, 5, 1920, RecordMode::Relative, "Recorded");
    CHECK(rel.find("note 0 r64") != std::string::npos);
    CHECK(parsePhrase(rel.data(), rel.size(), *p, err, sizeof err));
}
