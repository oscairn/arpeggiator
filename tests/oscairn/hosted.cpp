// SPDX-License-Identifier: MIT
// The arpeggiator hosted in Oscairn's engine (RFC 0032 step 2):
//   1. notes in, notes out, the same at 256- and 37-sample blocks, in the engine and in a worker;
//   2. a phrase pushed as SysEx by the plugin before it, through the graph, takes over slot 0 within
//      two blocks of its sample (the main thread parses it between blocks);
//   3. stopped transport: two arpeggiators with Sync Quantize, the second added 100 ms later, start on
//      the same sample (the free-running beat, oscairn.beat);
//   4. with an archive: a converted Performance installs as a library, loads with its arpeggiators
//      (the nodes with the role) and the engines (no stand-ins), plays repeated notes with every
//      arpeggiator on and one with them all off, and saves and loads again with the same states and
//      the same audio.
//   5. Chord Source Harmony: the chord held in a lower zone, given to the arpeggiator as its harmony
//      (oscairn.harmony) while its own input hears only the upper zone, drives the phrase; the key
//      in the upper zone starts it. Off, the arpeggiator plays its own key.
//   arp-hosted-test ARP.clap WORKER PUSHER.clap [ARCHIVE.olib PERFORMANCE_ID]
#include "oscairn/graph.h"
#include "oscairn/session.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;
using kick::clap::Event;

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++failures;
}

const char* kUp = "oscairn-phrase 1\nname \"up\"\nlength 480\ntrack\n  select up\n  note 0 p0 v100 g60\n  note 120 p0 v100 g60\n"
                  "  note 240 p0 v100 g60\n  note 360 p0 v100 g60\nend\n";

std::string stateText(const char* phrase, const std::string& params = "") {
    const std::string t = phrase;
    return "oscairn-arpeggiator 1\n" + params + "slot 0 " + std::to_string(t.size()) + "\n" + t + "\n";
}
std::string stateDir(const fs::path& root, const std::string& name, const std::string& text) {
    const auto d = root / name;
    fs::create_directories(d);
    std::ofstream(d / "plugin.id") << "org.oscairn.arpeggiator";
    std::ofstream(d / "state.clap", std::ios::binary) << text;
    return d.string();
}

struct Note { int64_t at; int key; bool operator==(const Note&) const = default; };
struct Recorder final : oscairn::Graph::Listener {
    std::array<oscairn::NodeId, 2> nodes{};
    int64_t blockStart = 0;
    std::array<std::vector<Note>, 2> ons;
    Recorder() { for (auto& o : ons) o.reserve(4096); }
    void events(oscairn::NodeId n, const Event* list, uint32_t total, bool emitted) noexcept override {
        if (!emitted) return;
        for (int k = 0; k < 2; ++k)
            if (n == nodes[k])
                for (uint32_t i = 0; i < total; ++i) {
                    const auto* h = list[i].header();
                    if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_NOTE_ON && ons[k].size() < ons[k].capacity())
                        ons[k].push_back({blockStart + h->time, reinterpret_cast<const clap_event_note_t*>(h)->key});
                }
    }
};

struct Input { int64_t at; Event e; };

// Renders `frames` of a graph made by `make`, feeding `input` at graphInput; returns each recorded node's note-ons.
template <class Make>
std::array<std::vector<Note>, 2> render(const std::string& worker, bool inWorker, uint32_t block, int64_t frames,
                                        std::vector<Input> input, Make make) {
    oscairn::setPlacementPolicy({inWorker ? worker : std::string(), oscairn::Placement::Worker});
    oscairn::Graph g(48000, block, oscairn::ProcessingMode::Offline);
    Recorder rec;
    make(g, rec, int64_t(0));
    g.setListener(&rec);
    std::vector<float> l(block), r(block);
    float* out[]{l.data(), r.data()};
    clap_event_transport_t tr{};
    tr.header = {sizeof tr, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    tr.flags = CLAP_TRANSPORT_HAS_TEMPO;
    tr.tempo = 120;
    size_t k = 0;
    for (int64_t at = 0; at < frames; at += block) {
        make(g, rec, at);
        std::vector<Event> ev;
        while (k < input.size() && input[k].at < at + block) {
            Event e = input[k].e;
            const_cast<clap_event_header_t*>(e.header())->time = uint32_t(input[k].at - at);
            ev.push_back(e);
            ++k;
        }
        rec.blockStart = at;
        if (!g.process(out, block, &tr, ev.data(), uint32_t(ev.size()))) check(false, "process");
        for (int n = 0; n < 2; ++n) if (rec.nodes[n]) g.host(rec.nodes[n]).service();  // main-thread callbacks
    }
    g.setListener(nullptr);
    return rec.ons;
}

std::string where(bool w, uint32_t b) { return std::string(w ? "worker" : "engine") + ", " + std::to_string(b) + "-sample blocks"; }
std::string keys(const std::vector<Note>& v, size_t n = 8) {
    std::string s;
    for (size_t i = 0; i < v.size() && i < n; ++i) s += std::to_string(v[i].key) + "@" + std::to_string(v[i].at) + " ";
    return s;
}

std::vector<float> play(oscairn::Session& s, int seconds, std::vector<int> keys = {48, 52, 55}) {
    std::vector<float> all, l(256), r(256);
    float* out[]{l.data(), r.data()};
    for (int k : keys) s.note(true, k, 0.8);
    for (int b = 0; b < seconds * 48000 / 256; ++b) {
        if (b == seconds * 48000 / 256 - 40) for (int k : keys) s.note(false, k, 0);
        s.process(out, 256);
        all.insert(all.end(), l.begin(), l.end());
        s.service();
    }
    return all;
}
// Onsets: the 10 ms level rising past half its recent peak after falling below a quarter of it.
int onsets(const std::vector<float>& x) {
    int n = 0;
    bool low = true;
    double peak = 1e-9;
    for (size_t i = 0; i + 480 <= x.size(); i += 480) {
        double e = 0;
        for (size_t j = i; j < i + 480; ++j) e += double(x[j]) * x[j];
        e = std::sqrt(e / 480);
        peak = std::max(peak * 0.97, e);
        if (low && e > 0.5 * peak && e > 1e-4) { ++n; low = false; }
        if (e < 0.25 * peak) low = true;
    }
    return n;
}
std::string slurp(const fs::path& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
}  // namespace

int main(int argc, char** argv) {
    if (argc != 4 && argc != 6) return std::fprintf(stderr, "usage: arp-hosted-test ARP.clap WORKER PUSHER.clap [ARCHIVE.olib PERFORMANCE_ID]\n"), 2;
    const std::string arp = argv[1], worker = argv[2], pusher = argv[3];
    const fs::path tmp0 = fs::temp_directory_path() / ("arp-hosted-" + std::to_string(::getpid()));
    fs::create_directories(tmp0);
    const fs::path tmp = fs::canonical(tmp0);  // the asset store wants a path without links (macOS's /var is one)
    const auto upState = stateDir(tmp, "up", stateText(kUp, "param 1 1\n"));

    {  // 1. notes in and out
        std::vector<Input> in = {{100, Event::note(true, 60, .8, 0, 0, 0, 1)}, {100, Event::note(true, 64, .8, 0, 0, 0, 2)},
                                 {48000, Event::note(false, 60, 0, 0, 0, 0, 1)}, {48000, Event::note(false, 64, 0, 0, 0, 0, 2)}};
        std::vector<Note> first;
        for (bool w : {false, true})
            for (uint32_t b : {256u, 37u}) {
                auto ons = render(worker, w, b, 72000, in, [&](oscairn::Graph& g, Recorder& rec, int64_t at) {
                    if (at) return;
                    const auto n = g.add(arp, "org.oscairn.arpeggiator");
                    g.host(n).loadState(upState);
                    g.connectNotes(oscairn::graphInput, n);
                    g.commit();
                    rec.nodes[0] = n;
                })[0];
                if (first.empty()) {
                    first = ons;
                    // Up over C and E, a sixteenth (6000 samples at 120 BPM) apart, for one second.
                    bool ok = ons.size() == 8;
                    for (size_t i = 0; ok && i < 8; ++i) ok = ons[i] == Note{100 + 6000 * int64_t(i), i % 2 ? 64 : 60};
                    check(ok, "notes out as the phrase says (" + where(w, b) + "): " + keys(ons));
                } else check(ons == first, "the same notes, " + where(w, b) + ": " + keys(ons));
            }
    }

    {  // 2. a pushed phrase through the graph
        std::vector<Input> in = {{3000, Event::note(true, 60, .8, 0, 0, 0, 1)}, {3000, Event::note(true, 64, .8, 0, 0, 0, 2)},
                                 {3000, Event::note(true, 67, .8, 0, 0, 0, 3)}};
        for (bool w : {false, true})
            for (uint32_t b : {256u, 37u}) {
                auto ons = render(worker, w, b, 20000, in, [&](oscairn::Graph& g, Recorder& rec, int64_t at) {
                    if (at) return;
                    const auto p = g.add(pusher, "org.oscairn.arpeggiator.test-pusher");
                    const auto n = g.add(arp, "org.oscairn.arpeggiator");
                    g.host(n).loadState(upState);
                    g.connectNotes(oscairn::graphInput, p);
                    g.connectNotes(p, n);
                    g.commit();
                    rec.nodes[0] = n;
                })[0];
                // Pushed at sample 1000; installed by the main thread after that block, so the chord at 3000 plays "down".
                check(ons.size() >= 3 && ons[0].key == 67 && ons[1].key == 64 && ons[2].key == 60 && ons[0].at == 3000,
                      "the pushed phrase plays (" + where(w, b) + "): " + keys(ons));
            }
    }

    {  // 3. two arpeggiators started 100 ms apart start together while stopped
        const auto syncState = stateDir(tmp, "sync", stateText(kUp, "param 1 1\nparam 27 7\n"));
        std::vector<Input> in = {{30000, Event::note(true, 60, .8, 0, 0, 0, 1)}};
        for (bool w : {false, true})
            for (uint32_t b : {256u, 37u}) {
                auto ons = render(worker, w, b, 60000, in, [&](oscairn::Graph& g, Recorder& rec, int64_t at) {
                    const int k = at == 0 ? 0 : (at >= 4800 && !rec.nodes[1]) ? 1 : -1;
                    if (k < 0) return;
                    const auto n = g.add(arp, "org.oscairn.arpeggiator");
                    g.host(n).loadState(syncState);
                    g.connectNotes(oscairn::graphInput, n);
                    g.commit();
                    rec.nodes[k] = n;
                });
                // The graph's beat starts at 0 with it: beat 1.25 at sample 30000, so the next line (beat 2) is 48000.
                check(!ons[0].empty() && !ons[1].empty() && ons[0][0].at == ons[1][0].at && ons[0][0].at == 48000,
                      "both start on the same beat line (" + where(w, b) + "): " + keys(ons[0], 1) + "/ " + keys(ons[1], 1));
            }
    }

    {  // 5. the chord from oscairn.harmony
        oscairn::NoteZone lower, upper;
        lower.highKey = 59;
        upper.lowKey = 60;
        std::vector<Input> in = {{100, Event::note(true, 48, .8, 0, 0, 0, 1)}, {100, Event::note(true, 52, .8, 0, 0, 0, 2)},
                                 {100, Event::note(true, 55, .8, 0, 0, 0, 3)}, {3000, Event::note(true, 84, .8, 0, 0, 0, 4)}};
        for (int source : {1, 0}) {
            const auto state = stateDir(tmp, "harmony" + std::to_string(source), stateText(kUp, "param 1 1\nparam 48 " + std::to_string(source) + "\n"));
            for (bool w : {false, true})
                for (uint32_t b : {256u, 37u}) {
                    auto ons = render(worker, w, b, 30000, in, [&](oscairn::Graph& g, Recorder& rec, int64_t at) {
                        if (at) return;
                        const auto n = g.add(arp, "org.oscairn.arpeggiator");
                        g.host(n).loadState(state);
                        g.connectNotes(oscairn::graphInput, n, {upper});
                        g.setHarmonies({{1, {n}, true, {lower}, 0, 1}});
                        g.commit();
                        rec.nodes[0] = n;
                    })[0];
                    const std::vector<Note> want = source ? std::vector<Note>{{3000, 48}, {9000, 52}, {15000, 55}, {21000, 48}, {27000, 52}}
                                                          : std::vector<Note>{{3000, 84}, {9000, 84}, {15000, 84}, {21000, 84}, {27000, 84}};
                    check(ons == want, std::string(source ? "Chord Source Harmony plays the lower zone's chord" : "Chord Source Own keys plays its own key") +
                                           " (" + where(w, b) + "): " + keys(ons));
                }
        }
    }

    if (argc == 6) {  // 4. a converted Performance, with any number of arpeggiators (one per arpeggiating Part)
        setenv("OSCAIRN_DATA_HOME", (tmp / "data").c_str(), 1);
        oscairn::setPlacementPolicy({std::string(), oscairn::Placement::Worker});
        // Every arpeggiator's state in a saved Performance, by path.
        auto arpStates = [](const fs::path& dir) {
            std::vector<std::pair<std::string, std::string>> v;
            for (const auto& e : fs::recursive_directory_iterator(dir))
                if (e.path().filename() == "state.clap") {
                    std::string t = slurp(e.path());
                    if (t.rfind("oscairn-arpeggiator 1\n", 0) == 0) v.emplace_back(e.path().lexically_relative(dir).string(), std::move(t));
                }
            std::sort(v.begin(), v.end());
            return v;
        };
        std::vector<float> fresh;
        std::vector<std::pair<std::string, std::string>> saved;
        size_t arpCount = 0;
        {
            oscairn::Session s(48000, 256);
            const auto inst = s.installArchive(argv[4], true, {}, true);
            check(inst.added > 0, "the archive installs (" + std::to_string(inst.added) + " items)");
            for (const auto& n : inst.notes) std::printf("      note: %s\n", n.c_str());
            s.loadPerformanceFromLibrary(argv[5]);
            const auto st = s.sessionStatus();
            check(st.failures.empty(), "no plugin failed");
            for (const auto& f : st.failures) std::printf("      failure: %s: %s\n", f.name.c_str(), f.reason.c_str());
            // The arpeggiators: the nodes with the role (RFC 0032); for a conversion from before the
            // converter wrote roles, the nodes named "... Arp".
            const auto graph = s.graphState();
            std::vector<oscairn::NodeId> arps;
            for (const auto& [id, role] : graph.roles) if (role == "arpeggiator") arps.push_back(id);
            if (arps.empty())
                for (const auto& [id, name] : graph.names)
                    if (name.size() > 4 && name.compare(name.size() - 4, 4, " Arp") == 0) arps.push_back(id);
            for (const auto& [id, name] : graph.names)
                std::printf("      node %u: %s%s\n", id, name.c_str(), std::count(arps.begin(), arps.end(), id) ? " (arpeggiator)" : "");
            arpCount = arps.size();
            check(!arps.empty(), "the Performance has arpeggiators (" + std::to_string(arps.size()) + ")");
            auto switchAll = [&](int on) {
                bool ok = !arps.empty();
                for (const auto a : arps) ok = s.setParameter(a, 1, on) && ok;
                s.service();
                return ok;
            };
            check(switchAll(1), "switch every arpeggio on");
            fresh = play(s, 3);  // before anything else plays: the reloaded Performance is compared with it
            check(onsets(fresh) >= 1, "the mix sounds with every arpeggio on");
            s.savePerformance((tmp / "saved").string());
            saved = arpStates(tmp / "saved");
            bool ok = saved.size() == arps.size();
            for (const auto& [path, text] : saved) ok = ok && text.find("slot ") != std::string::npos && text.find("param 1 1\n") != std::string::npos;
            check(ok, "the saved Performance holds every arpeggiator's state with its phrases (" + std::to_string(saved.size()) + " of " + std::to_string(arps.size()) + ")");
            play(s, 2, {});
            // Each arpeggiating part alone (soloed), with a chord inside its own zone (parts of a split
            // hear different keys), its arpeggio off: one chord, no repeated notes. (Repeated notes with
            // it on are counted below as the notes the arpeggiator sends, since legato steps on a
            // sustained sound leave the level without dips.)
            bool quiet = true;
            for (const auto& part : s.parts()) {
                const auto a = std::find_if(part.chain.begin(), part.chain.end(), [&](oscairn::NodeId n) { return std::count(arps.begin(), arps.end(), n); });
                if (a == part.chain.end() || part.input.zones.empty()) continue;
                const int lo = part.input.zones[0].lowKey, hi = part.input.zones[0].highKey;
                const int base = std::clamp(60, lo, std::max(lo, hi - 7));
                std::vector<int> chord;
                for (int k : {base, base + 4, base + 7}) if (k <= hi) chord.push_back(k);
                s.setPartSolo(part.id, true);
                s.setPartLevel(part.id, part.level, false);
                s.commit();  // solo and level are staged graph edits
                s.setParameter(*a, 1, 0);
                s.service();
                const int off = onsets(play(s, 3, chord));
                s.setParameter(*a, 1, 1);
                s.setPartSolo(part.id, false);
                s.setPartLevel(part.id, part.level, part.mute);
                s.commit();
                s.service();
                play(s, 1, {});
                std::printf("      part %u %s (keys %d-%d): %d onsets with its arpeggio off\n", part.id, part.name.c_str(), lo, hi, off);
                quiet = quiet && off <= 2;
            }
            check(quiet, "no part arpeggiates with its switch off");
        }
        {   // Each arpeggiator as saved, in a graph of its own, on a three- and a two-key chord (a
            // phrase plays only for its held counts): the busiest sends repeated notes.
            int most = 0;
            for (const auto& [path, text] : saved) {
                const auto dir = (tmp / "saved" / path).parent_path().string();
                for (int held : {3, 2}) {
                    std::vector<Input> in;
                    for (int i = 0; i < held; ++i) in.push_back({100, Event::note(true, 60 + (i == 0 ? 0 : i == 1 ? 4 : 7), .8, 0, 0, 0, i + 1)});
                    const auto ons = render(worker, false, 256, 134400, in, [&](oscairn::Graph& g, Recorder& rec, int64_t at) {
                        if (at) return;
                        const auto n = g.add(arp, "org.oscairn.arpeggiator");
                        g.host(n).loadState(dir);
                        g.connectNotes(oscairn::graphInput, n);
                        g.commit();
                        rec.nodes[0] = n;
                    })[0];
                    std::printf("      %s: %zu notes in 2.8 s from %d held keys: %s\n", path.c_str(), ons.size(), held, keys(ons).c_str());
                    most = std::max<int>(most, int(ons.size()));
                }
            }
            check(most >= 8, "arpeggiated: " + std::to_string(most) + " notes in 2.8 s from one held chord by the busiest arpeggiator");
        }
        {
            oscairn::Session s(48000, 256);
            s.loadPerformance((tmp / "saved").string());
            check(s.sessionStatus().failures.empty(), "loads again with no failure");
            const auto again = play(s, 3);
            double diff = 0, energy = 0;
            for (size_t i = 0; i < std::min(again.size(), fresh.size()); ++i) { diff = std::max(diff, double(std::fabs(again[i] - fresh[i]))); energy += fresh[i] * fresh[i]; }
            check(energy > 1e-3 && diff < 1e-6, "the reloaded Performance plays the same (largest difference " + std::to_string(diff) + ")");
            s.savePerformance((tmp / "saved2").string());
            const auto second = arpStates(tmp / "saved2");
            check(second.size() == arpCount && second == saved, "every arpeggiator's state round-trips byte for byte");
        }
    }
    fs::remove_all(tmp);
    std::printf("%s (%d failed)\n", failures ? "FAIL" : "ok", failures);
    return failures ? 1 : 0;
}
