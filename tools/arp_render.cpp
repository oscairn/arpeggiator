// SPDX-License-Identifier: MIT
// Offline render of a phrase with given settings and played keys, printed as timed events.
//   arp-render PHRASE [options] EVENT...
//   options: --tempo BPM (120)  --rate HZ (48000)  --block FRAMES (256)  --seconds S (8)  --seed N
//            --set NAME=VALUE  (hold, keymode, velmode, trigger, loop, playonly, unit, qvalue, qstrength,
//            swing, velrate, gaterate, octrange, outoct, notelo, notehi, vello, velhi, accent, accentq,
//            sfx, sfxvel, sfxkey, sdbd, sync, change)
//   events (seconds): on@T:KEY:VEL  off@T:KEY  cc@T:64:VALUE
//   output: "on SECONDS KEY VELOCITY7 VELOCITY16 CHANNEL", "off SECONDS KEY", "cc SECONDS NUM VALUE",
//           "bend SECONDS VALUE", "pressure SECONDS VALUE"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "player.h"

namespace {
struct In { double t; int kind; int a, b; };  // kind 0 on, 1 off, 2 sustain

struct Print : arp::Sink {
    double rate = 48000;
    uint64_t base = 0;
    bool push(const arp::Output& o) override {
        const double t = double(base + o.frame) / rate;
        switch (o.type) {
            case arp::Output::NoteOn: std::printf("on %.6f %d %d %d %d\n", t, o.key, o.value, o.velocity, o.channel + 1); break;
            case arp::Output::NoteOff: std::printf("off %.6f %d\n", t, o.key); break;
            case arp::Output::Control: std::printf("cc %.6f %d %d\n", t, o.key, o.value); break;
            case arp::Output::Bend: std::printf("bend %.6f %d\n", t, o.bend); break;
            case arp::Output::Pressure: std::printf("pressure %.6f %d\n", t, o.value); break;
        }
        return true;
    }
};

bool set(arp::Settings& s, const std::string& kv) {
    const auto eq = kv.find('=');
    if (eq == std::string::npos) return false;
    const std::string k = kv.substr(0, eq);
    const int v = std::atoi(kv.c_str() + eq + 1);
    if (k == "hold") s.hold = arp::HoldMode(std::clamp(v, 0, 2));
    else if (k == "keymode") s.keyMode = arp::KeyMode(std::clamp(v, 0, 4));
    else if (k == "velmode") s.velocityMode = arp::VelocityMode(v != 0);
    else if (k == "trigger") s.trigger = arp::TriggerMode(v != 0);
    else if (k == "loop") s.loop = v != 0;
    else if (k == "playonly") s.playOnly = v != 0;
    else if (k == "unit") s.unit = std::clamp(v, 0, 9);
    else if (k == "qvalue") s.quantizeTicks = v;
    else if (k == "qstrength") s.quantizeStrength = v;
    else if (k == "swing") s.swing = v;
    else if (k == "velrate") s.velocityRate = v;
    else if (k == "gaterate") s.gateRate = v;
    else if (k == "octrange") s.octaveRange = v;
    else if (k == "outoct") s.outputOctave = v;
    else if (k == "notelo") s.noteLow = v;
    else if (k == "notehi") s.noteHigh = v;
    else if (k == "vello") s.velocityLow = v;
    else if (k == "velhi") s.velocityHigh = v;
    else if (k == "accent") s.accentThreshold = v;
    else if (k == "accentq") s.accentQuantize = v != 0;
    else if (k == "sfx") s.randomSfx = v != 0;
    else if (k == "sfxvel") s.sfxVelocityOffset = v;
    else if (k == "sfxkey") s.sfxKeyVelocity = v != 0;
    else if (k == "sdbd") s.fixedSdBd = v != 0;
    else if (k == "sync") s.syncQuantize = v;
    else if (k == "change") s.changeTiming = arp::ChangeTiming(v != 0);
    else return false;
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: arp-render PHRASE [options] EVENT...\n"); return 2; }
    std::ifstream f(argv[1]);
    if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    auto phrase = std::make_unique<arp::Phrase>();
    char err[200];
    if (!arp::parsePhrase(text.data(), text.size(), *phrase, err, sizeof err)) { std::fprintf(stderr, "%s: %s\n", argv[1], err); return 1; }
    arp::Settings s;
    arp::Clock clock;
    double seconds = 8;
    int block = 256;
    uint32_t seed = 1;
    std::vector<In> in;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", a.c_str()); std::exit(2); } return argv[++i]; };
        if (a == "--tempo") clock.tempo = std::atof(next().c_str());
        else if (a == "--rate") clock.sampleRate = std::atof(next().c_str());
        else if (a == "--block") block = std::max(1, std::atoi(next().c_str()));
        else if (a == "--seconds") seconds = std::atof(next().c_str());
        else if (a == "--seed") seed = uint32_t(std::strtoul(next().c_str(), nullptr, 10));
        else if (a == "--set") { if (!set(s, next())) { std::fprintf(stderr, "bad --set\n"); return 2; } }
        else {
            In e{};
            const auto at = a.find('@');
            if (at == std::string::npos) { std::fprintf(stderr, "bad event %s\n", a.c_str()); return 2; }
            const std::string kind = a.substr(0, at);
            int x = 0, y = 0;
            const char* p = a.c_str() + at + 1;
            e.t = std::atof(p);
            const char* c1 = std::strchr(p, ':');
            if (c1) { x = std::atoi(c1 + 1); const char* c2 = std::strchr(c1 + 1, ':'); if (c2) y = std::atoi(c2 + 1); }
            if (kind == "on") e = {e.t, 0, x, y};
            else if (kind == "off") e = {e.t, 1, x, 0};
            else if (kind == "cc" && x == 64) e = {e.t, 2, 64, y};
            else { std::fprintf(stderr, "bad event %s\n", a.c_str()); return 2; }
            in.push_back(e);
        }
    }
    std::stable_sort(in.begin(), in.end(), [](const In& a, const In& b) { return a.t < b.t; });
    arp::Player player;
    player.seed(seed);
    player.setPhrase(0, phrase.get());
    player.setSettings(s);
    Print out;
    out.rate = clock.sampleRate;
    const uint64_t total = uint64_t(seconds * clock.sampleRate);
    size_t next = 0;
    for (uint64_t pos = 0; pos < total; pos += block) {
        const uint32_t n = uint32_t(std::min<uint64_t>(block, total - pos));
        out.base = pos;
        player.begin(clock, n, out);
        while (next < in.size() && uint64_t(in[next].t * clock.sampleRate + 0.5) < pos + n) {
            const auto& e = in[next++];
            const uint32_t fr = uint32_t(std::max<int64_t>(0, int64_t(e.t * clock.sampleRate + 0.5) - int64_t(pos)));
            if (e.kind == 0) player.keyOn(fr, 0, e.a, e.b, -1);
            else if (e.kind == 1) player.keyOff(fr, 0, e.a, -1);
            else player.sustain(fr, 0, e.b >= 64);
        }
        player.end();
    }
    return 0;
}
