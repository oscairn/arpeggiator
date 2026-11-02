// SPDX-License-Identifier: MIT
// One test per playback rule, on synthetic phrases. At 120 BPM and 48 kHz one tick (480 per quarter)
// is exactly 50 frames.
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "player.h"

using namespace arp;

namespace {

constexpr int kTick = 50;

struct Ev { int type; long frame; int key; int value; int velocity16; int channel; int bend; int32_t inputId; };
enum { ON, OFF, CC, BEND, PRESS };

struct Collect : Sink {
    std::vector<Ev> events;
    long base = 0;
    bool push(const Output& o) override {
        events.push_back({o.type == Output::NoteOn ? ON : o.type == Output::NoteOff ? OFF : o.type == Output::Control ? CC : o.type == Output::Bend ? BEND : PRESS,
                          base + long(o.frame), o.key, o.value, o.velocity, o.channel, o.bend, o.inputId});
        return true;
    }
};

struct Key { long tick; int kind; int key; int vel; };  // kind: 0 on, 1 off, 2 pedal
Key on(long t, int k, int v = 100) { return {t, 0, k, v}; }
Key off(long t, int k) { return {t, 1, k, 0}; }
Key pedal(long t, bool down) { return {t, 2, 64, down ? 127 : 0}; }

std::unique_ptr<Phrase> phrase(const std::string& body, const char* header = "length 1920") {
    auto p = std::make_unique<Phrase>();
    const std::string text = std::string("oscairn-phrase 1\n") + header + "\n" + body;
    char err[160] = {};
    if (!parsePhrase(text.data(), text.size(), *p, err, sizeof err)) { std::printf("  phrase error: %s\n", err); ++g_failures; }
    return p;
}

std::vector<Ev> render(const Phrase& ph, const Settings& s, std::vector<Key> keys, long ticks, int block = 256,
                       const Phrase* other = nullptr, std::vector<std::pair<long, Settings>> changes = {}) {
    Player p;
    p.setPhrase(0, &ph);
    if (other) p.setPhrase(1, other);
    p.setSettings(s);
    Collect c;
    Clock clock;
    const long total = ticks * kTick;
    std::stable_sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.tick < b.tick; });
    size_t k = 0, ch = 0;
    for (long pos = 0; pos < total; pos += block) {
        const uint32_t n = uint32_t(std::min<long>(block, total - pos));
        c.base = pos;
        while (ch < changes.size() && changes[ch].first * kTick < pos + n && changes[ch].first * kTick <= pos) p.setSettings(changes[ch++].second);
        p.begin(clock, n, c);
        while (k < keys.size() && keys[k].tick * kTick < pos + n) {
            const uint32_t f = uint32_t(keys[k].tick * kTick - pos);
            if (keys[k].kind == 0) p.keyOn(f, 0, keys[k].key, keys[k].vel, int32_t(k));
            else if (keys[k].kind == 1) p.keyOff(f, 0, keys[k].key, -1);
            else p.sustain(f, 0, keys[k].vel >= 64);
            ++k;
        }
        p.end();
    }
    return c.events;
}

std::vector<int> onKeys(const std::vector<Ev>& ev, bool passThrough = false) {
    std::vector<int> out;
    for (const auto& e : ev) if (e.type == ON && (passThrough || e.inputId < 0)) out.push_back(e.key);
    return out;
}
std::vector<long> onTicks(const std::vector<Ev>& ev) {
    std::vector<long> out;
    for (const auto& e : ev) if (e.type == ON && e.inputId < 0) out.push_back(e.frame);
    return out;
}
std::vector<int> v(std::initializer_list<int> l) { return l; }

// Sixteen 1/16 notes, each at the given note-list position (p0 by default).
std::string steps(const char* select, std::initializer_list<const char*> sources, int count = 8, int gate = 60) {
    std::string s = std::string("track\n  select ") + select + "\n";
    int i = 0;
    for (int n = 0; n < count; ++n) {
        const char* src = sources.size() ? sources.begin()[n % sources.size()] : "p0";
        s += "  note " + std::to_string(n * 120) + " " + src + " v100 g" + std::to_string(gate) + "\n";
        ++i;
    }
    return s + "end\n";
}
const std::vector<Key> kTriad = {on(0, 60), on(0, 64), on(0, 67)};

}  // namespace

TEST(index_plays_positions_and_skips_missing) {
    auto ph = phrase(steps("index", {"p0", "p1", "p2", "p3"}, 4), "length 480");
    CHECK(onKeys(render(*ph, {}, kTriad, 480)) == v({60, 64, 67}));
}

TEST(index_last_clamps_and_index_first_falls_back) {
    auto last = phrase(steps("index-last", {"p0", "p3"}, 2), "length 240");
    CHECK(onKeys(render(*last, {}, kTriad, 240)) == v({60, 67}));
    auto first = phrase(steps("index-first", {"p1", "p5"}, 2), "length 240");
    CHECK(onKeys(render(*first, {}, kTriad, 240)) == v({64, 60}));
    // The fallback is the first held key even when template notes come first in the list.
    auto withTemplate = phrase("track\n  select index-first\n  keys template+held\n  template 48 a36\n  note 0 p9 v100 g60\nend\n", "length 240");
    CHECK(onKeys(render(*withTemplate, {}, kTriad, 240)) == v({60}));
}

TEST(bounce_bounce2_and_cycle_positions) {
    auto b = phrase(steps("bounce", {"p0", "p1", "p2", "p3", "p4", "p5"}, 6), "length 720");
    CHECK(onKeys(render(*b, {}, kTriad, 720)) == v({60, 64, 67, 64, 60, 64}));
    auto b2 = phrase(steps("bounce2", {"p0", "p1", "p2", "p3", "p4", "p5"}, 6), "length 720");
    CHECK(onKeys(render(*b2, {}, kTriad, 720)) == v({60, 64, 67, 67, 64, 60}));
    auto c = phrase(steps("cycle", {"p3", "p4", "p5"}, 3), "length 360");
    CHECK(onKeys(render(*c, {}, kTriad, 360)) == v({60, 64, 67}));
}

TEST(lift_raises_positions_past_the_threshold) {
    auto ph = phrase("track\n  select index-last\n  lift 1\n  note 0 p0 v100 g60\n  note 120 p3 v100 g60\nend\n", "length 240");
    // p0 below the lift plays as is; p3 clamps to the last key (no lift on non-zero positions).
    CHECK(onKeys(render(*ph, {}, {on(0, 60)}, 240)) == v({60, 72}));
}

TEST(walker_up_crosses_its_octaves_and_continues_across_loops) {
    auto ph = phrase("track\n  select up\n  octaves 2\n" + std::string("  note 0 p0 v100 g60\n  note 120 p0 v100 g60\n  note 240 p0 v100 g60\n  note 360 p0 v100 g60\nend\n"), "length 480");
    CHECK(onKeys(render(*ph, {}, kTriad, 960)) == v({60, 64, 67, 72, 76, 79, 60, 64}));
}

TEST(walker_down_updown_and_updown2) {
    auto d = phrase(steps("down", {}, 4), "length 480");
    CHECK(onKeys(render(*d, {}, kTriad, 480)) == v({67, 64, 60, 67}));
    auto ud = phrase(steps("updown", {}, 8), "length 960");
    CHECK(onKeys(render(*ud, {}, kTriad, 960)) == v({60, 64, 67, 64, 60, 64, 67, 64}));
    auto ud2 = phrase(steps("updown2", {}, 8), "length 960");
    CHECK(onKeys(render(*ud2, {}, kTriad, 960)) == v({60, 64, 67, 67, 64, 60, 60, 64}));
}

TEST(walker_random_stays_in_the_list_and_is_deterministic) {
    auto r = phrase(steps("random", {}, 8) , "length 960");
    const auto a = onKeys(render(*r, {}, kTriad, 960)), b = onKeys(render(*r, {}, kTriad, 960));
    CHECK(a == b);
    CHECK(a.size() == 8);
    for (int k : a) CHECK(k == 60 || k == 64 || k == 67);
    CHECK(std::count(a.begin(), a.end(), a[0]) < 8);
}

TEST(tracks_play_only_within_their_held_count) {
    auto ph = phrase("track\n  held 1 1\n  note 0 k40 v100 g60\nend\ntrack\n  held 2 2\n  note 0 k50 v100 g60\nend\n"
                     "track\n  held 3 any\n  note 0 k70 v100 g60\nend\n", "length 480");
    CHECK(onKeys(render(*ph, {}, {on(0, 60)}, 480)) == v({40}));
    CHECK(onKeys(render(*ph, {}, {on(0, 60), on(0, 62)}, 480)) == v({50}));
    CHECK(onKeys(render(*ph, {}, {on(0, 60), on(0, 62), on(0, 65), on(0, 69)}, 480)) == v({70}));
}

TEST(template_notes_follow_the_chord_and_snap_to_its_degrees) {
    // Template written in C; the minor 3rd and the 4th snap to the held major 3rd; 36 stays absolute.
    auto ph = phrase("track\n  keys template+held\n  template 60 c60 c63 c65 a36\n" + std::string(
                     "  note 0 p0 v100 g60\n  note 120 p1 v100 g60\n  note 240 p2 v100 g60\n  note 360 p3 v100 g60\n"
                     "  note 480 p4 v100 g60\nend\n"), "length 960");
    CHECK(onKeys(render(*ph, {}, {on(0, 62), on(0, 66), on(0, 69)}, 960)) == v({62, 66, 66, 36, 62}));
    auto upper = phrase("track\n  keys template+upper\n  template 60 a36\n  note 0 p1 v100 g60\n  note 120 p2 v100 g60\nend\n", "length 240");
    CHECK(onKeys(render(*upper, {}, kTriad, 240)) == v({64, 67}));
}

TEST(key_mode_thru_keeps_played_order) {
    auto ph = phrase(steps("index", {"p0", "p1", "p2"}, 3), "length 360");
    Settings s;
    s.keyMode = KeyMode::Thru;
    CHECK(onKeys(render(*ph, s, {on(0, 67), on(0, 60), on(0, 64)}, 360)) == v({67, 60, 64}));
    s.keyMode = KeyMode::Sort;
    CHECK(onKeys(render(*ph, s, {on(0, 67), on(0, 60), on(0, 64)}, 360)) == v({60, 64, 67}));
}

TEST(direct_passes_keys_and_plays_only_controllers) {
    auto ph = phrase("track\n  note 0 p0 v100 g60\n  cc 0 74 20\nend\n", "length 480");
    Settings s;
    s.keyMode = KeyMode::Direct;
    const auto ev = render(*ph, s, {on(0, 60), off(200, 60)}, 480);
    CHECK(onKeys(ev, true) == v({60}));
    CHECK(ev[0].inputId == 0);
    CHECK(std::count_if(ev.begin(), ev.end(), [](const Ev& e) { return e.type == CC && e.key == 74 && e.value == 20; }) == 1);
    s.keyMode = KeyMode::SortDirect;
    CHECK(onKeys(render(*ph, s, {on(0, 60)}, 480), true) == v({60, 60}));
}

TEST(velocity_original_thru_rates_and_clamps) {
    auto ph = phrase("track\n  note 0 p0 v80 g60\nend\n", "length 480");
    Settings s;
    auto vel = [&](int keyVel) { const auto ev = render(*ph, s, {on(0, 60, keyVel)}, 480); return ev.empty() ? -1 : ev[0].value; };
    CHECK_EQ(vel(30), 80);
    s.velocityMode = VelocityMode::Thru;
    CHECK_EQ(vel(30), 30);
    s.velocityMode = VelocityMode::Original;
    s.velocityRate = 50;
    CHECK_EQ(vel(30), 40);
    s.velocityRate = 0;
    CHECK_EQ(vel(30), 1);
    s.velocityRate = 200;
    CHECK_EQ(vel(30), 127);
    s.velocityRate = 100;
    s.slotVelocityOffset[0] = -50;
    CHECK_EQ(vel(30), 40);
}

TEST(gate_rate_scales_gates_with_a_one_tick_minimum) {
    auto ph = phrase("track\n  note 0 p0 v80 g100\nend\n", "length 480");
    Settings s;
    auto len = [&]() { const auto ev = render(*ph, s, {on(0, 60)}, 480); return ev.size() < 2 ? -1 : ev[1].frame - ev[0].frame; };
    CHECK_EQ(len(), 100 * kTick);
    s.gateRate = 50;
    CHECK_EQ(len(), 50 * kTick);
    s.gateRate = 0;
    CHECK_EQ(len(), 1 * kTick);
    s.gateRate = 100;
    s.slotGateOffset[0] = 100;
    CHECK_EQ(len(), 200 * kTick);
}

TEST(unit_multiply_scales_time_exactly) {
    auto ph = phrase(steps("index", {"p0"}, 2), "length 240");
    Settings s;
    s.unit = 6;  // 200 %
    auto t = onTicks(render(*ph, s, kTriad, 480));
    CHECK(t.size() >= 2 && t[1] - t[0] == 240 * kTick);
    s.unit = 1;  // 66 %: two thirds
    t = onTicks(render(*ph, s, kTriad, 480));
    CHECK(t.size() >= 2 && t[1] - t[0] == 80 * kTick);
}

TEST(quantize_strength_and_swing) {
    auto ph = phrase("track\n  note 0 p0 v80 g10\n  note 100 p0 v80 g10\n  note 240 p0 v80 g10\nend\n", "length 480");
    Settings s;
    s.quantizeTicks = 120;
    s.quantizeStrength = 100;
    CHECK(onTicks(render(*ph, s, {on(0, 60)}, 480)) == std::vector<long>({0, 120 * kTick, 240 * kTick}));
    s.quantizeStrength = 50;
    CHECK(onTicks(render(*ph, s, {on(0, 60)}, 480)) == std::vector<long>({0, 110 * kTick, 240 * kTick}));
    s.quantizeStrength = 100;
    s.swing = 40;  // the second grid position is late
    CHECK(onTicks(render(*ph, s, {on(0, 60)}, 480)) == std::vector<long>({0, 160 * kTick, 240 * kTick}));
}

TEST(octave_range_climbs_each_pass_and_widens_walkers) {
    auto ph = phrase("track\n  note 0 p0 v80 g10\nend\n", "length 120");
    Settings s;
    s.octaveRange = 2;
    CHECK(onKeys(render(*ph, s, {on(0, 60)}, 480)) == v({60, 72, 84, 60}));
    s.octaveRange = -1;
    CHECK(onKeys(render(*ph, s, {on(0, 60)}, 360)) == v({60, 48, 60}));
    auto up = phrase(steps("up", {}, 4), "length 480");
    s.octaveRange = 1;
    CHECK(onKeys(render(*up, s, {on(0, 60), on(0, 64)}, 480)) == v({60, 64, 72, 76}));
}

TEST(walker_span_has_no_ceiling_beyond_the_parameters) {
    std::string body = "track\n  select up\n  octaves 4\n";
    for (int i = 0; i < 7; ++i) body += "  note " + std::to_string(i * 60) + " p0 v100 g10\n";
    auto ph = phrase(body + "end\n", "length 480");
    Settings s;
    s.octaveRange = 3;  // four walked octaves plus three: seven octaves from key 24
    CHECK(onKeys(render(*ph, s, {on(0, 24)}, 420)) == v({24, 36, 48, 60, 72, 84, 96}));
}

TEST(output_octave_moves_chord_notes_but_not_absolute_template_notes) {
    auto ph = phrase("track\n  keys template+held\n  template 60 a36\n  note 0 p0 v100 g60\n  note 120 p1 v100 g60\nend\n", "length 240");
    Settings s;
    s.outputOctave = 1;
    CHECK(onKeys(render(*ph, s, {on(0, 60)}, 240)) == v({36, 72}));
}

TEST(hold_off_stops_at_release) {
    auto ph = phrase(steps("index", {"p0"}, 4, 200), "length 480");
    const auto ev = render(*ph, {}, {on(0, 60), off(130, 60)}, 960);
    CHECK(onKeys(ev).size() == 2);
    CHECK(ev.back().type == OFF && ev.back().frame == 130 * kTick);  // the sounding note ends with the arp
}

TEST(hold_on_keeps_playing_and_a_new_gesture_replaces_the_chord) {
    auto ph = phrase(steps("index", {"p0"}, 4, 30), "length 480");
    Settings s;
    s.hold = HoldMode::On;
    const auto ev = render(*ph, s, {on(0, 60), off(10, 60), on(450, 62), off(460, 62)}, 900);
    CHECK(onKeys(ev) == v({60, 60, 60, 60, 62, 62, 62, 62}));
}

TEST(sync_off_runs_silently_and_resumes_in_place) {
    auto ph = phrase(steps("index", {"p0", "p0", "p0", "p0"}, 4, 30), "length 480");
    Settings s;
    s.hold = HoldMode::SyncOff;
    // Released during the second step, pressed again during the fourth: the phrase did not restart.
    const auto t = onTicks(render(*ph, s, {on(0, 60), off(130, 60), on(300, 62)}, 480));
    CHECK(t == std::vector<long>({0, 120 * kTick, 360 * kTick}));
    s.hold = HoldMode::Off;
    const auto t2 = onTicks(render(*ph, s, {on(0, 60), off(130, 60), on(300, 62)}, 480));
    CHECK(t2 == std::vector<long>({0, 120 * kTick, 300 * kTick, 420 * kTick}));
}

TEST(toggle_trigger_starts_and_stops_with_gestures) {
    auto ph = phrase(steps("index", {"p0"}, 4, 30), "length 480");
    Settings s;
    s.trigger = TriggerMode::Toggle;
    CHECK(onKeys(render(*ph, s, {on(0, 60), off(10, 60), on(250, 60), off(260, 60)}, 960)).size() == 3);
}

TEST(loop_off_plays_one_pass) {
    auto ph = phrase(steps("index", {"p0"}, 4, 30), "length 480");
    Settings s;
    s.loop = false;
    CHECK(onKeys(render(*ph, s, {on(0, 60)}, 1440)).size() == 4);
}

TEST(note_and_velocity_limits_with_inverted_ranges) {
    auto ph = phrase("track\n  note 0 p0 v100 g60\nend\n", "length 480");
    Settings s;
    s.noteLow = 72;  // above high: arp below 60 and above 72
    s.noteHigh = 59;
    const auto ev = render(*ph, s, {on(0, 65)}, 480);
    CHECK(ev.size() >= 1 && ev[0].inputId == 0);  // passed through, no arp note
    CHECK(onKeys(ev).empty());
    CHECK(onKeys(render(*ph, s, {on(0, 50)}, 480)) == v({50}));
    Settings vs;
    vs.velocityLow = 90;
    vs.velocityHigh = 40;
    CHECK(onKeys(render(*ph, vs, {on(0, 60, 60)}, 480)).empty());
    CHECK(onKeys(render(*ph, vs, {on(0, 60, 100)}, 480)) == v({60}));
}

TEST(play_only_silences_played_keys) {
    auto ph = phrase("track\n  note 0 p0 v100 g60\nend\n", "length 480");
    Settings s;
    s.keyMode = KeyMode::SortDirect;
    s.playOnly = true;
    CHECK(onKeys(render(*ph, s, {on(0, 60)}, 480), true) == v({60}));
}

TEST(accent_switches_to_accent_tracks_for_one_pass) {
    auto ph = phrase("track\n  note 0 k40 v100 g10\n  note 240 k40 v100 g10\nend\ntrack\n  accent\n  note 0 k90 v100 g10\nend\n", "length 480");
    Settings s;
    s.accentThreshold = 100;
    s.accentQuantize = false;
    CHECK(onKeys(render(*ph, s, {on(0, 60, 80)}, 960)) == v({40, 40, 40, 40}));
    CHECK(onKeys(render(*ph, s, {on(0, 60, 110)}, 960)) == v({90, 40, 40}));
    s.accentThreshold = 0;
    CHECK(onKeys(render(*ph, s, {on(0, 60, 127)}, 480)) == v({40, 40}));
}

TEST(random_sfx_plays_at_release_within_its_ranges) {
    auto ph = phrase("track\n  note 0 p0 v100 g60\nend\n", "length 480\nsfx 120 122 30 50 100");
    Settings s;
    s.sfxKeyVelocity = false;
    const auto ev = render(*ph, s, {on(0, 60, 90), off(200, 60)}, 480);
    const auto keys = onKeys(ev);
    CHECK(keys.size() == 2 && keys[1] >= 120 && keys[1] <= 122);
    CHECK(ev.size() >= 3);
    for (const auto& e : ev) if (e.type == ON && e.key >= 120) CHECK(e.value >= 30 && e.value <= 50);
    // The SFX note lasts its own gate even though the release stopped the arpeggio.
    long sfxOn = -1, sfxOff = -1;
    for (const auto& e : ev) if (e.key >= 120) (e.type == ON ? sfxOn : sfxOff) = e.frame;
    CHECK(sfxOn == 200 * kTick && sfxOff == 300 * kTick);
    s.sfxKeyVelocity = true;
    s.sfxVelocityOffset = 10;
    for (const auto& e : render(*ph, s, {on(0, 60, 90), off(200, 60)}, 480)) if (e.type == ON && e.key >= 120) CHECK_EQ(e.value, 100);
    s.randomSfx = false;
    CHECK(onKeys(render(*ph, s, {on(0, 60, 90), off(200, 60)}, 480)).size() == 1);
}

TEST(fixed_sd_bd_maps_kick_and_snare_variants) {
    auto ph = phrase("track\n  keys template+held\n  template 48 a35 a40 a42\n  note 0 p0 v100 g10\n  note 0 p1 v100 g10\n  note 0 p2 v100 g10\nend\n", "length 480");
    Settings s;
    s.fixedSdBd = true;
    auto k = onKeys(render(*ph, s, {on(0, 60)}, 100));
    std::sort(k.begin(), k.end());
    CHECK(k == v({36, 38, 42}));
}

TEST(controllers_need_the_held_count_and_the_chord_filter) {
    auto ph = phrase("track\n  held 2 any\n  cc 0 1 64\n  bend 0 100\nend\ntrack\n  chords 0x001 0x7ffffffff\n  cc 10 2 5\nend\n", "length 480");
    auto count = [](const std::vector<Ev>& ev, int type) { return std::count_if(ev.begin(), ev.end(), [type](const Ev& e) { return e.type == type; }); };
    auto one = render(*ph, {}, {on(0, 62)}, 480);
    CHECK(count(one, BEND) == 0);
    CHECK(count(one, CC) == 0);  // root D is outside the second track's C-only filter
    auto two = render(*ph, {}, {on(0, 60), on(0, 64), on(0, 67)}, 480);
    CHECK(count(two, BEND) == 1);
    CHECK(count(two, CC) == 2);
}

TEST(change_timing_measure_waits_for_the_pass_end) {
    auto a = phrase(steps("index", {"p0"}, 4, 30), "length 480");
    auto b = phrase("track\n  note 0 k90 v100 g30\n  note 120 k90 v100 g30\n  note 240 k90 v100 g30\n  note 360 k90 v100 g30\nend\n", "length 480");
    Settings s, t;
    t.slot = 1;
    auto keys = onKeys(render(*a, s, {on(0, 60)}, 960, 50, b.get(), {{130, t}}));
    CHECK(keys == v({60, 60, 60, 60, 90, 90, 90, 90}));
    s.changeTiming = t.changeTiming = ChangeTiming::Realtime;
    keys = onKeys(render(*a, s, {on(0, 60)}, 960, 50, b.get(), {{130, t}}));
    CHECK(keys == v({60, 60, 90, 90, 90, 90, 90, 90}));
}

TEST(sync_quantize_waits_for_the_grid) {
    auto ph = phrase(steps("index", {"p0"}, 2, 30), "length 240");
    Settings s;
    s.syncQuantize = 480;
    const auto t = onTicks(render(*ph, s, {on(100, 60)}, 960));
    CHECK(!t.empty() && t[0] == 480 * kTick);
}

TEST(output_does_not_depend_on_block_size) {
    auto ph = phrase("track\n  select updown\n  octaves 2\n  note 0 p0 v90 g70\n  note 90 p0 v80 g200\n  note 333 p0 v70 g5\n  cc 400 7 9\nend\n", "length 480");
    Settings s;
    s.swing = 30;
    s.quantizeStrength = 60;
    s.octaveRange = 1;
    const std::vector<Key> keys = {on(3, 60), on(17, 64), off(700, 64), on(900, 67), off(1500, 60)};
    const auto a = render(*ph, s, keys, 2000, 37), b = render(*ph, s, keys, 2000, 512);
    CHECK(a.size() == b.size());
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); ++i) same = a[i].type == b[i].type && a[i].frame == b[i].frame && a[i].key == b[i].key && a[i].value == b[i].value;
    CHECK(same);
}

TEST(randomize_moves_keys_within_its_window) {
    auto ph = phrase("track\n  randomize 0 127 0 127 0 2 0 0\n" + std::string(
                     "  note 0 p0 v100 g10\n  note 60 p0 v100 g10\n  note 120 p0 v100 g10\n  note 180 p0 v100 g10\n"
                     "  note 240 p0 v100 g10\n  note 300 p0 v100 g10\nend\n"), "length 480");
    const auto k = onKeys(render(*ph, {}, {on(0, 60)}, 480));
    CHECK(k.size() == 6);
    for (int x : k) CHECK(x >= 60 && x <= 62);
    CHECK(std::count(k.begin(), k.end(), 60) < 6);
}

TEST(relative_and_fallback_sources) {
    auto ph = phrase("track\n  root 60\n  note 0 r67 v100 g10\n  note 120 p3/k30 v100 g10\n  note 240 p0/k30 v100 g10\nend\n", "length 480");
    CHECK(onKeys(render(*ph, {}, {on(0, 62)}, 480)) == v({69, 30, 62}));
}

TEST(style_tracks_move_source_chords_to_the_held_chord) {
    // Written for C major 7: C E G B. Held D minor 7 (D F A C): root moves up 2, the 3rd and 7th flatten.
    auto ph = phrase("track\n  select style\n  style 0 2\n  note 0 k60 v100 g10\n  note 0 k64 v100 g10\n  note 0 k67 v100 g10\n"
                     "  note 0 k71 v100 g10\nend\n", "length 480");
    auto k = onKeys(render(*ph, {}, {on(0, 50), on(0, 53), on(0, 57), on(0, 60)}, 100));
    std::sort(k.begin(), k.end());
    CHECK(k == v({62, 65, 69, 72}));
}

// Enough events that a sort which buffers (std::stable_sort does above a small size) would allocate.
static std::string manyControllers() {
    std::string t = "track\n";
    for (int i = 0; i < 400; ++i) t += "  cc " + std::to_string(i) + " 1 " + std::to_string(i % 128) + "\n";
    return t + "end\n";
}

TEST(processing_never_allocates) {
    if (!g_allocationCounting) { std::printf("  skipped: allocations cannot be counted in this build\n"); return; }
    auto ph = phrase("track\n  select updown\n  octaves 3\n  keys template+held\n  template 60 c64 a36\n  note 0 p0 v90 g70\n"
                     "  note 90 p1 v80 g200\n  cc 100 1 1\nend\ntrack\n  select style\n  note 0 k64 v100 g30\nend\n"
                     "track\n  accent\n  note 0 p0 v100 g30\nend\n" + manyControllers(), "length 480\nsfx 100 110 1 127 50");
    auto other = phrase(steps("random", {}, 4), "length 480");
    Settings s;
    s.accentThreshold = 64;
    s.swing = 20;
    s.quantizeStrength = 50;
    s.octaveRange = 2;
    Settings t = s;
    t.slot = 1;
    t.quantizeStrength = 80;
    g_allocations = 0;
    // render() itself allocates its vectors before and after; count only inside the player's calls.
    Player p;
    p.setPhrase(0, ph.get());
    p.setPhrase(1, other.get());
    Collect c;
    c.events.reserve(100000);
    Clock clock;
    g_countAllocations = true;
    p.setSettings(s);
    for (long pos = 0, i = 0; pos < 4000 * kTick; pos += 128, ++i) {
        if (i == 300) p.setSettings(t);
        p.begin(clock, 128, c);
        if (i % 20 == 1) p.keyOn(5, 0, 60 + int(i % 7), 70 + int(i % 50), -1);
        if (i % 20 == 15) p.keyOff(9, 0, 60 + int((i - 14) % 7), -1);
        if (i == 700) p.sustain(0, 0, true);
        if (i == 900) p.sustain(0, 0, false);
        p.end();
    }
    p.panic(0);
    g_countAllocations = false;
    CHECK_EQ(g_allocations, 0);
    CHECK(c.events.size() > 100);
}
