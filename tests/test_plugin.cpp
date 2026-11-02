// SPDX-License-Identifier: MIT
// Loads the built plugin and checks it as a host would: descriptor and ports, parameters, notes in and
// out at any block size, state round trip with phrases, phrase assets (found, or missing with the embedded
// copy playing), phrases pushed on the note input, and main-thread calls running while another thread
// processes (for ThreadSanitizer).
#include <clap/clap.h>
#include <dlfcn.h>
#include <oscairn-clap/assets.h>
#include <oscairn-clap/beat.h>
#include <oscairn-clap/harmony.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { ++failures; std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); } } while (0)

namespace {

std::atomic<int> callbacks{0};
const clap_host_t kHost = {CLAP_VERSION, nullptr, "test", "oscairn", "", "1",
                           [](const clap_host_t*, const char*) -> const void* { return nullptr; },
                           [](const clap_host_t*) {}, [](const clap_host_t*) {},
                           [](const clap_host_t*) { ++callbacks; }};
// A host that gives the free-running beat (oscairn.beat) in event space 0x4321.
constexpr uint16_t kBeatSpace = 0x4321;
const clap_host_event_registry_t kRegistry = {[](const clap_host_t*, const char* space, uint16_t* id) {
    if (std::strcmp(space, OSCAIRN_EVENT_SPACE_BEAT)) return false;
    *id = kBeatSpace;
    return true;
}};
const clap_host_t kBeatHost = {CLAP_VERSION, nullptr, "test", "oscairn", "", "1",
                               [](const clap_host_t*, const char* id) -> const void* { return std::strcmp(id, CLAP_EXT_EVENT_REGISTRY) ? nullptr : &kRegistry; },
                               [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) { ++callbacks; }};

// A host that gives the part's harmony (oscairn.harmony) in event space 0x4322.
constexpr uint16_t kHarmonySpace = 0x4322;
const clap_host_event_registry_t kHarmonyRegistry = {[](const clap_host_t*, const char* space, uint16_t* id) {
    if (std::strcmp(space, OSCAIRN_EVENT_SPACE_HARMONY)) return false;
    *id = kHarmonySpace;
    return true;
}};
const clap_host_t kHarmonyHost = {CLAP_VERSION, nullptr, "test", "oscairn", "", "1",
                                  [](const clap_host_t*, const char* id) -> const void* { return std::strcmp(id, CLAP_EXT_EVENT_REGISTRY) ? nullptr : &kHarmonyRegistry; },
                                  [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) { ++callbacks; }};
oscairn_event_harmony_t harmonyEvent(std::initializer_list<int> keys) {
    oscairn_event_harmony_t e{};
    e.header = {sizeof e, 0, kHarmonySpace, OSCAIRN_EVENT_HARMONY, 0};
    for (int k : keys) e.harmony.held[k / 64] |= uint64_t(1) << (k % 64);
    return e;
}

// A host with an asset store (oscairn.assets) holding one file, assetFile; empty: nothing resolves.
std::string assetFile;
const oscairn_host_assets_t kAssets = {[](const clap_host_t*, oscairn_asset_ref_t* ref, char* path, uint32_t cap, char* msg, uint32_t msgCap) {
    if (assetFile.empty()) return std::snprintf(msg, msgCap, "%s not found", ref->path), false;
    std::snprintf(path, cap, "%s", assetFile.c_str());
    if (!ref->sha256[0]) std::memset(ref->sha256, 'a', 64);  // the host fills in an unknown hash
    return true;
}};
const clap_host_t kAssetHost = {CLAP_VERSION, nullptr, "test", "oscairn", "", "1",
                                [](const clap_host_t*, const char* id) -> const void* { return std::strcmp(id, OSCAIRN_EXT_ASSETS) ? nullptr : &kAssets; },
                                [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) { ++callbacks; }};

struct InEvents {
    std::vector<std::vector<uint8_t>> events;
    clap_input_events_t api{this, [](const clap_input_events_t* l) { return uint32_t(static_cast<const InEvents*>(l->ctx)->events.size()); },
                            [](const clap_input_events_t* l, uint32_t i) {
                                return reinterpret_cast<const clap_event_header_t*>(static_cast<const InEvents*>(l->ctx)->events[i].data());
                            }};
    template <class T> void add(const T& e) { events.emplace_back(reinterpret_cast<const uint8_t*>(&e), reinterpret_cast<const uint8_t*>(&e) + sizeof e); }
};
struct OutEvents {
    struct Note { int type; long frame; int key; int channel; int id; double velocity; };
    std::vector<Note> notes;
    long base = 0;
    clap_output_events_t api{this, [](const clap_output_events_t* l, const clap_event_header_t* h) {
        auto* o = static_cast<OutEvents*>(l->ctx);
        if (h->type == CLAP_EVENT_NOTE_EXPRESSION) {
            const auto* n = reinterpret_cast<const clap_event_note_expression_t*>(h);
            o->notes.push_back({h->type, o->base + h->time, n->key, n->channel, n->note_id, n->value});
        }
        if (h->type == CLAP_EVENT_NOTE_ON || h->type == CLAP_EVENT_NOTE_OFF || h->type == CLAP_EVENT_NOTE_END || h->type == CLAP_EVENT_NOTE_CHOKE) {
            const auto* n = reinterpret_cast<const clap_event_note_t*>(h);
            o->notes.push_back({h->type, o->base + h->time, n->key, n->channel, n->note_id, n->velocity});
        }
        return true;
    }};
};

clap_event_note_t note(uint16_t type, uint32_t time, int key, double vel = 0.8, int id = -1) {
    clap_event_note_t e{};
    e.header = {sizeof e, time, CLAP_CORE_EVENT_SPACE_ID, type, 0};
    e.note_id = id; e.port_index = 0; e.channel = 0; e.key = int16_t(key); e.velocity = vel;
    return e;
}
clap_event_param_value_t param(clap_id id, double v) {
    clap_event_param_value_t e{};
    e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    e.param_id = id; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1; e.value = v;
    return e;
}

struct Instance {
    const clap_plugin_t* p;
    const clap_plugin_params_t* params;
    const clap_plugin_state_t* state;
    double beatOffset = -1;  // >= 0: send the free-running beat, pos / 24000 + beatOffset, each block
    Instance(const clap_plugin_factory_t* f, const clap_host_t* host = &kHost) {
        p = f->create_plugin(f, host, "org.oscairn.arpeggiator");
        p->init(p);
        params = static_cast<const clap_plugin_params_t*>(p->get_extension(p, CLAP_EXT_PARAMS));
        state = static_cast<const clap_plugin_state_t*>(p->get_extension(p, CLAP_EXT_STATE));
        p->activate(p, 48000, 1, 4096);
        p->start_processing(p);
    }
    ~Instance() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
    // Renders `frames`, delivering each timed input in the block that contains it.
    void run(OutEvents& out, std::vector<std::pair<long, std::vector<uint8_t>>> in, long frames, uint32_t block, long start = 0, long playingUntil = -1,
             long jumpAt = -1) {
        size_t k = 0;
        for (long pos = start; pos < start + frames; pos += block) {
            const uint32_t n = uint32_t(std::min<long>(block, start + frames - pos));
            InEvents ev;
            if (beatOffset >= 0) {
                oscairn_event_beat_t be{};
                be.header = {sizeof be, 0, kBeatSpace, OSCAIRN_EVENT_BEAT, 0};
                be.beats = clap_beattime(std::llround((pos / 24000.0 + beatOffset) * CLAP_BEATTIME_FACTOR));
                be.tempo = 120;
                ev.add(be);
            }
            while (k < in.size() && in[k].first < pos + n) {
                ev.events.push_back(in[k].second);
                reinterpret_cast<clap_event_header_t*>(ev.events.back().data())->time = uint32_t(std::max<long>(0, in[k].first - pos));
                ++k;
            }
            out.base = pos;
            clap_event_transport_t tr{};
            tr.header = {sizeof tr, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
            tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | (pos < playingUntil ? CLAP_TRANSPORT_IS_PLAYING : 0);
            // Beats follow the frames (120 BPM, 48 kHz), plus ten beats once past jumpAt.
            const double beats = pos / 24000.0 + (jumpAt >= 0 && pos >= jumpAt ? 10 : 0);
            tr.song_pos_beats = clap_beattime(std::llround(beats * CLAP_BEATTIME_FACTOR));
            tr.tempo = 120;
            clap_process_t b{};
            b.steady_time = pos; b.frames_count = n; b.transport = &tr; b.in_events = &ev.api; b.out_events = &out.api;
            p->process(p, &b);
        }
    }
};
template <class T> std::pair<long, std::vector<uint8_t>> at(long frame, const T& e) {
    return {frame, std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(&e), reinterpret_cast<const uint8_t*>(&e) + sizeof e)};
}

struct Stream {
    std::string data;
    size_t pos = 0;
    clap_ostream_t out{this, [](const clap_ostream_t* s, const void* b, uint64_t n) -> int64_t {
        static_cast<Stream*>(s->ctx)->data.append(static_cast<const char*>(b), n); return int64_t(n); }};
    clap_istream_t in{this, [](const clap_istream_t* s, void* b, uint64_t n) -> int64_t {
        auto* st = static_cast<Stream*>(s->ctx);
        const size_t k = std::min<size_t>(n, std::min<size_t>(7, st->data.size() - st->pos));  // short reads on purpose
        std::memcpy(b, st->data.data() + st->pos, k); st->pos += k; return int64_t(k); }};
};

const char* kUp = "oscairn-phrase 1\nname \"up\"\nlength 480\ntrack\n  select up\n  note 0 p0 v100 g60\n  note 120 p0 v100 g60\n"
                  "  note 240 p0 v100 g60\n  note 360 p0 v100 g60\nend\n";

std::string stateWith(const char* phrase, int slot = 0) {
    std::string t = phrase;
    return "oscairn-arpeggiator 1\nslot " + std::to_string(slot) + " " + std::to_string(t.size()) + "\n" + t + "\n";
}
bool load(Instance& in, const std::string& s) {
    Stream st;
    st.data = s;
    return in.state->load(in.p, &st.in);
}
std::vector<int> ons(const OutEvents& o) {
    std::vector<int> k;
    for (auto& n : o.notes) if (n.type == CLAP_EVENT_NOTE_ON) k.push_back(n.key);
    return k;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: plugin_tests PLUGIN.clap\n"); return 2; }
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { std::printf("dlopen: %s\n", dlerror()); return 1; }
    auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
    CHECK(entry && entry->init(argv[1]));
    auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    CHECK(factory && factory->get_plugin_count(factory) == 1);
    CHECK(!std::strcmp(factory->get_plugin_descriptor(factory, 0)->id, "org.oscairn.arpeggiator"));

    {  // ports and parameters
        Instance in(factory);
        auto* audio = static_cast<const clap_plugin_audio_ports_t*>(in.p->get_extension(in.p, CLAP_EXT_AUDIO_PORTS));
        auto* notes = static_cast<const clap_plugin_note_ports_t*>(in.p->get_extension(in.p, CLAP_EXT_NOTE_PORTS));
        CHECK(audio && audio->count(in.p, true) == 0 && audio->count(in.p, false) == 0);
        CHECK(notes && notes->count(in.p, true) == 1 && notes->count(in.p, false) == 1);
        const uint32_t n = in.params->count(in.p);
        CHECK(n == 48);
        std::vector<clap_id> ids;
        for (uint32_t i = 0; i < n; ++i) {
            clap_param_info_t info;
            CHECK(in.params->get_info(in.p, i, &info));
            CHECK(info.min_value <= info.default_value && info.default_value <= info.max_value);
            for (clap_id id : ids) CHECK(id != info.id);
            ids.push_back(info.id);
        }
        char text[64];
        CHECK(in.params->value_to_text(in.p, 8, 6, text, sizeof text) && !std::strcmp(text, "200%"));
    }

    {  // notes in and out, independent of block size
        std::vector<int> keys[2];
        std::vector<long> frames[2];
        const uint32_t blocks[2] = {37, 512};
        for (int b = 0; b < 2; ++b) {
            Instance in(factory);
            CHECK(load(in, stateWith(kUp)));
            in.p->on_main_thread(in.p);
            OutEvents out;
            in.run(out, {at(100, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1)), at(100, note(CLAP_EVENT_NOTE_ON, 0, 64, 0.8, 2)),
                         at(48000, note(CLAP_EVENT_NOTE_OFF, 0, 60, 0, 1)), at(48000, note(CLAP_EVENT_NOTE_OFF, 0, 64, 0, 2))},
                   72000, blocks[b]);
            keys[b] = ons(out);
            for (auto& x : out.notes) frames[b].push_back(x.frame);
            int ends = 0;
            for (auto& x : out.notes) ends += x.type == CLAP_EVENT_NOTE_END;
            CHECK(ends == 2);
        }
        CHECK(keys[0] == keys[1] && frames[0] == frames[1]);
        CHECK(keys[0].size() == 8 && keys[0][0] == 60 && keys[0][1] == 64 && keys[0][2] == 60);
    }

    {  // without a phrase or with the switch off, keys pass straight through
        Instance in(factory);
        OutEvents out;
        in.run(out, {at(10, note(CLAP_EVENT_NOTE_ON, 0, 61, 0.5, 7)), at(500, note(CLAP_EVENT_NOTE_OFF, 0, 61, 0, 7))}, 1000, 128);
        CHECK(ons(out) == std::vector<int>({61}));
        Instance off(factory);
        CHECK(load(off, stateWith(kUp)));
        off.p->on_main_thread(off.p);
        OutEvents o2;
        off.run(o2, {at(0, param(1, 0)), at(10, note(CLAP_EVENT_NOTE_ON, 0, 62, 0.5, 7))}, 48000, 256);
        CHECK(ons(o2) == std::vector<int>({62}));
    }

    {  // a passed-through key keeps its expressions, under its output identity
        Instance in(factory);
        clap_event_note_expression_t ex{};
        ex.header = {sizeof ex, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_EXPRESSION, 0};
        ex.expression_id = CLAP_NOTE_EXPRESSION_PRESSURE; ex.note_id = 7; ex.port_index = 0; ex.channel = 0; ex.key = 61; ex.value = 0.5;
        OutEvents out;
        in.run(out, {at(10, note(CLAP_EVENT_NOTE_ON, 0, 61, 0.5, 7)), at(20, ex)}, 256, 256);
        int onId = -2, exId = -3;
        for (auto& n : out.notes) { if (n.type == CLAP_EVENT_NOTE_ON) onId = n.id; if (n.type == CLAP_EVENT_NOTE_EXPRESSION) exId = n.id; }
        CHECK(onId == exId);
    }

    {  // state round trip keeps parameters and phrases; bad states are refused
        Instance a(factory);
        CHECK(load(a, stateWith(kUp, 3)));
        OutEvents o;
        a.run(o, {at(0, param(8, 6)), at(0, param(28, 3))}, 64, 64);
        Stream st;
        CHECK(a.state->save(a.p, &st.out));
        Instance b(factory);
        Stream st2;
        st2.data = st.data;
        CHECK(b.state->load(b.p, &st2.in));
        double v = 0;
        CHECK(b.params->get_value(b.p, 8, &v) && v == 6);
        Stream st3;
        CHECK(b.state->save(b.p, &st3.out));
        CHECK(st3.data == st.data);
        CHECK(st.data.find("slot 3") != std::string::npos);
        CHECK(!load(b, "nonsense"));
        CHECK(!load(b, stateWith("oscairn-phrase 1\ntrack\n  note 0 q1 v1 g1\nend\n")));
        CHECK(!load(b, "oscairn-arpeggiator 1\nslot 0 99999\nshort"));
    }

    {  // a slot's phrase asset: declared optional; missing, the embedded copy plays; found, it replaces the copy
        const std::string sha(64, 'c');
        const std::string withAsset = "oscairn-arpeggiator 1\nasset 0 user " + sha + " 0123/My Up.ophr\n" + stateWith(kUp).substr(22);
        auto play = [](Instance& in) {
            in.p->on_main_thread(in.p);
            OutEvents out;
            in.run(out, {at(0, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1)), at(0, note(CLAP_EVENT_NOTE_ON, 0, 64, 0.8, 2))}, 24000, 256);
            return ons(out);
        };
        Instance in(factory, &kAssetHost);
        assetFile.clear();
        CHECK(load(in, withAsset));
        CHECK(play(in) == std::vector<int>({60, 64, 60, 64}));  // missing: the embedded "up"
        auto* deps = static_cast<const oscairn_plugin_asset_dependencies_t*>(in.p->get_extension(in.p, OSCAIRN_EXT_ASSET_DEPENDENCIES));
        CHECK(deps && deps->count(in.p) == 1 && deps->optional && deps->optional(in.p, 0) && !deps->optional(in.p, 1));
        oscairn_asset_ref_t r{};
        CHECK(deps && deps->get(in.p, 0, &r) && !std::strcmp(r.library, "user") && !std::strcmp(r.path, "0123/My Up.ophr") && r.sha256 == sha);
        Stream st;
        CHECK(in.state->save(in.p, &st.out) && st.data.find("asset 0 user " + sha + " 0123/My Up.ophr\n") != std::string::npos);

        // Found: the asset file plays ("down"), and a hash the state did not give is filled in.
        assetFile = std::string(argv[1]) + ".test-asset.ophr";
        std::ofstream(assetFile) << "oscairn-phrase 1\nlength 480\ntrack\n  select down\n  note 0 p0 v100 g60\n  note 120 p0 v100 g60\n"
                                    "  note 240 p0 v100 g60\n  note 360 p0 v100 g60\nend\n";
        Instance found(factory, &kAssetHost);
        CHECK(load(found, "oscairn-arpeggiator 1\nasset 0 user - x/Down.ophr\nasset 1 user - x/Down.ophr\n" + stateWith(kUp).substr(22)));
        CHECK(play(found) == std::vector<int>({64, 60, 64, 60}));
        Stream st2;
        CHECK(found.state->save(found.p, &st2.out) && st2.data.find("asset 0 user " + std::string(64, 'a') + " x/Down.ophr\n") != std::string::npos);
        CHECK(deps && deps->count(found.p) == 1);  // two slots, one asset
        std::remove(assetFile.c_str());
        assetFile.clear();

        // A state without the line, or a phrase loaded over the slot, leaves no asset.
        CHECK(load(found, stateWith(kUp)) && deps && deps->count(found.p) == 0);
        CHECK(!load(found, "oscairn-arpeggiator 1\nasset 0 user short x.ophr\n"));
    }

    {  // Chord Source Harmony: the part's chord from oscairn.harmony drives the phrase; off, the own keys do
        char text[64];
        Instance in(factory, &kHarmonyHost);
        CHECK(in.params->value_to_text(in.p, 48, 1, text, sizeof text) && !std::strcmp(text, "Harmony"));
        CHECK(load(in, stateWith(kUp)));
        in.p->on_main_thread(in.p);
        OutEvents out;
        in.run(out, {at(0, param(48, 1)), at(0, harmonyEvent({48, 52, 55})), at(0, note(CLAP_EVENT_NOTE_ON, 0, 84, 0.8, 1))}, 24000, 256);
        CHECK(ons(out) == std::vector<int>({48, 52, 55, 48}));
        Stream st;
        CHECK(in.state->save(in.p, &st.out) && st.data.find("param 48 1\n") != std::string::npos);
        Instance own(factory, &kHarmonyHost);
        CHECK(load(own, stateWith(kUp)));
        own.p->on_main_thread(own.p);
        OutEvents o2;
        own.run(o2, {at(0, harmonyEvent({48, 52, 55})), at(0, note(CLAP_EVENT_NOTE_ON, 0, 84, 0.8, 1))}, 24000, 256);
        CHECK(ons(o2) == std::vector<int>({84, 84, 84, 84}));
    }

    {  // the transport stopping ends what sounds
        Instance in(factory);
        CHECK(load(in, stateWith(kUp)));
        in.p->on_main_thread(in.p);
        OutEvents out;
        in.run(out, {at(0, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1))}, 12000, 256, 0, 6000);
        int chokes = 0;
        long when = -1;
        for (auto& n : out.notes) if (n.type == CLAP_EVENT_NOTE_CHOKE) { ++chokes; when = n.frame; }
        CHECK(chokes == 1 && when >= 6000 && when < 6000 + 256);
    }

    {  // record mode: one bar of played notes becomes the selected slot's phrase
        Instance in(factory);
        OutEvents out;
        // Arm, then play C and E together and G an eighth later (24000 frames per quarter at 120 BPM, 48 kHz).
        in.run(out, {at(0, param(45, 1)), at(1000, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1)), at(1000, note(CLAP_EVENT_NOTE_ON, 0, 64, 0.8, 2)),
                     at(13000, note(CLAP_EVENT_NOTE_ON, 0, 67, 0.8, 3)), at(20000, note(CLAP_EVENT_NOTE_OFF, 0, 67, 0, 3)),
                     at(30000, note(CLAP_EVENT_NOTE_OFF, 0, 60, 0, 1)), at(30000, note(CLAP_EVENT_NOTE_OFF, 0, 64, 0, 2))}, 100000, 256);
        CHECK(ons(out) == std::vector<int>({60, 64, 67}));  // played through while recording
        double v = 1;
        CHECK(in.params->get_value(in.p, 45, &v) && v == 0);  // switched itself off after the bar
        in.p->on_main_thread(in.p);
        Stream st;
        CHECK(in.state->save(in.p, &st.out));
        CHECK(st.data.find("note 0 p0 v") != std::string::npos);
        CHECK(st.data.find("note 240 r67") != std::string::npos);
        OutEvents o2;
        in.run(o2, {at(0, note(CLAP_EVENT_NOTE_ON, 0, 62, 0.8, 9)), at(0, note(CLAP_EVENT_NOTE_ON, 0, 66, 0.8, 10))}, 20000, 256, 100000);
        CHECK(ons(o2) == std::vector<int>({62, 66, 69}));  // the recorded phrase, on a D chord
    }

    {  // a relocation while playing starts the phrase again
        Instance in(factory);
        CHECK(load(in, stateWith(kUp)));
        in.p->on_main_thread(in.p);
        OutEvents out;
        in.run(out, {at(0, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1)), at(0, note(CLAP_EVENT_NOTE_ON, 0, 64, 0.8, 2)),
                     at(0, note(CLAP_EVENT_NOTE_ON, 0, 67, 0.8, 3))}, 16000, 256, 0, 1 << 30, 8960);
        std::vector<long> f;
        for (auto& n : out.notes) if (n.type == CLAP_EVENT_NOTE_ON) f.push_back(n.frame);
        CHECK(ons(out) == std::vector<int>({60, 64, 60, 64}));
        CHECK(f.size() == 4 && f[2] == 8960 && f[3] == 14960);
    }

    {  // stopped transport: Sync Quantize lines up on the host's free-running beat, not each instance's own clock
        long first[2] = {-1, -1};
        const long starts[2] = {0, 7000};  // the second instance starts processing later
        for (int i = 0; i < 2; ++i) {
            Instance in(factory, &kBeatHost);
            in.beatOffset = 0.3;
            CHECK(load(in, stateWith(kUp)));
            in.p->on_main_thread(in.p);
            OutEvents out;
            in.run(out, {at(starts[i], param(27, 7)), at(30000, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1))}, 60000 - starts[i], 256, starts[i]);
            for (auto& n : out.notes) if (n.type == CLAP_EVENT_NOTE_ON && first[i] < 0) first[i] = n.frame;
        }
        // Beat 1.55 at frame 30000; the next whole beat, 2, is at frame (2 - 0.3) * 24000.
        CHECK(first[0] == 40800 && first[1] == 40800);
    }

    {  // a phrase pushed on the note input is installed by the main thread
        Instance in(factory);
        std::vector<uint8_t> sx = {0xF0, 0x7D, 'O', 'P', 'H', 'R', 0};
        sx.insert(sx.end(), kUp, kUp + std::strlen(kUp));
        sx.push_back(0xF7);
        clap_event_midi_sysex_t e{};
        e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI_SYSEX, 0};
        e.port_index = 0; e.buffer = sx.data(); e.size = uint32_t(sx.size());
        OutEvents out;
        const int before = callbacks.load();
        in.run(out, {at(0, e)}, 256, 256);
        CHECK(callbacks.load() > before);
        in.p->on_main_thread(in.p);
        in.run(out, {at(0, note(CLAP_EVENT_NOTE_ON, 0, 60, 0.8, 1)), at(0, note(CLAP_EVENT_NOTE_ON, 0, 67, 0.8, 2))}, 24000, 256, 256);
        CHECK(ons(out).size() == 4 && ons(out)[1] == 67);
    }

    {  // main-thread calls race with processing on another thread
        Instance in(factory);
        CHECK(load(in, stateWith(kUp)));
        in.p->on_main_thread(in.p);
        std::atomic<bool> done{false};
        std::thread audio([&] {
            OutEvents out;
            std::vector<std::pair<long, std::vector<uint8_t>>> evs;
            for (long f = 0; f < 480000; f += 9000) {
                evs.push_back(at(f, note(CLAP_EVENT_NOTE_ON, 0, 60 + int(f / 9000 % 12), 0.7, int(f))));
                evs.push_back(at(f + 4000, note(CLAP_EVENT_NOTE_OFF, 0, 60 + int(f / 9000 % 12), 0, int(f))));
            }
            in.run(out, evs, 480000, 128);
            done = true;
        });
        int saves = 0;
        while (!done) {
            double v;
            in.params->get_value(in.p, 12, &v);
            Stream st;
            in.state->save(in.p, &st.out);
            in.p->on_main_thread(in.p);
            Stream ld;
            ld.data = stateWith(kUp, saves % 8);
            in.state->load(in.p, &ld.in);
            ++saves;
        }
        audio.join();
        in.p->on_main_thread(in.p);
        CHECK(saves > 0);
    }

    entry->deinit();
    dlclose(lib);
    std::printf("%s (%d failed checks)\n", failures ? "FAIL" : "ok", failures);
    return failures ? 1 : 0;
}
