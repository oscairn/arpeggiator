// SPDX-License-Identifier: MIT
// The arpeggiator as a CLAP note effect: one note input, one note output, no audio.
// Threads: the audio thread owns the Player; the main thread owns phrase texts and allocation. A new
// phrase reaches the audio thread through a per-slot pending pointer; the replaced one comes back
// through a per-slot retired pointer and is freed on the main thread.
#include <clap/clap.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "phrase.h"
#include "player.h"
#include "record.h"

namespace {

struct ParamInfo { clap_id id; const char* name; const char* module; double min, max, def; const char* const* texts; };

const char* const kOffOn[] = {"Off", "On"};
const char* const kHold[] = {"Sync-Off", "Off", "On"};
const char* const kKeyMode[] = {"Sort", "Thru", "Direct", "Sort+Direct", "Thru+Direct"};
const char* const kVelMode[] = {"Original", "Thru"};
const char* const kTrigger[] = {"Gate", "Toggle"};
const char* const kUnit[] = {"50%", "66%", "75%", "100%", "133%", "150%", "200%", "266%", "300%", "400%"};
const char* const kGrid[] = {"60", "80", "120", "160", "240", "320", "480"};
const char* const kSync[] = {"Off", "60", "80", "120", "160", "240", "320", "480"};
const char* const kChange[] = {"Real-time", "Measure"};
const char* const kRecord[] = {"Off", "Armed"};
const char* const kConvert[] = {"Fixed", "Relative", "Chord"};
const int kGridTicks[] = {60, 80, 120, 160, 240, 320, 480};

enum : clap_id {
    pSwitch = 1, pHold, pKeyMode, pVelMode, pTrigger, pLoop, pPlayOnly, pUnit, pQuantize, pStrength, pSwing,
    pVelRate, pGateRate, pOctRange, pOutOct, pNoteLow, pNoteHigh, pVelLow, pVelHigh, pChange, pAccent, pAccentQ,
    pSfx, pSfxVel, pSfxKey, pSdBd, pSync, pSlot, pSlotVel1, pSlotGate1 = pSlotVel1 + 8, pRecord = pSlotGate1 + 8,
    pRecordBars, pRecordConvert, pEnd
};

const ParamInfo kBase[] = {
    {pSwitch, "Switch", "Arpeggio", 0, 1, 1, kOffOn},
    {pHold, "Hold", "Arpeggio", 0, 2, 1, kHold},
    {pKeyMode, "Key Mode", "Arpeggio", 0, 4, 0, kKeyMode},
    {pVelMode, "Velocity Mode", "Arpeggio", 0, 1, 0, kVelMode},
    {pTrigger, "Trigger", "Arpeggio", 0, 1, 0, kTrigger},
    {pLoop, "Loop", "Arpeggio", 0, 1, 1, kOffOn},
    {pPlayOnly, "Play Only", "Arpeggio", 0, 1, 0, kOffOn},
    {pUnit, "Unit Multiply", "Timing", 0, 9, 3, kUnit},
    {pQuantize, "Quantize Value", "Timing", 0, 6, 2, kGrid},
    {pStrength, "Quantize Strength", "Timing", 0, 100, 0, nullptr},
    {pSwing, "Swing", "Timing", -120, 120, 0, nullptr},
    {pVelRate, "Velocity Rate", "Arpeggio", 0, 200, 100, nullptr},
    {pGateRate, "Gate Rate", "Arpeggio", 0, 200, 100, nullptr},
    {pOctRange, "Octave Range", "Arpeggio", -3, 3, 0, nullptr},
    {pOutOct, "Output Octave", "Arpeggio", -10, 10, 0, nullptr},
    {pNoteLow, "Note Low", "Limits", 0, 127, 0, nullptr},
    {pNoteHigh, "Note High", "Limits", 0, 127, 127, nullptr},
    {pVelLow, "Velocity Low", "Limits", 1, 127, 1, nullptr},
    {pVelHigh, "Velocity High", "Limits", 1, 127, 127, nullptr},
    {pChange, "Change Timing", "Timing", 0, 1, 1, kChange},
    {pAccent, "Accent Threshold", "Accent and SFX", 0, 127, 0, nullptr},
    {pAccentQ, "Accent Quantize", "Accent and SFX", 0, 1, 1, kOffOn},
    {pSfx, "Random SFX", "Accent and SFX", 0, 1, 1, kOffOn},
    {pSfxVel, "SFX Velocity Offset", "Accent and SFX", -64, 63, 0, nullptr},
    {pSfxKey, "SFX Key Velocity", "Accent and SFX", 0, 1, 1, kOffOn},
    {pSdBd, "Fixed SD/BD", "Arpeggio", 0, 1, 0, kOffOn},
    {pSync, "Sync Quantize", "Timing", 0, 7, 0, kSync},
    {pSlot, "Slot", "Phrases", 0, 7, 0, nullptr},
};
constexpr int kBaseCount = int(sizeof kBase / sizeof kBase[0]);
const ParamInfo kRecordParams[] = {
    {pRecord, "Record", "Record", 0, 1, 0, kRecord},
    {pRecordBars, "Record Bars", "Record", 1, 8, 1, nullptr},
    {pRecordConvert, "Record Convert", "Record", 0, 2, 2, kConvert},
};
constexpr int kParamCount = kBaseCount + 16 + 3;

bool paramInfo(int index, ParamInfo& p, char* name, size_t nameSize) {
    if (index < 0 || index >= kParamCount) return false;
    if (index < kBaseCount) { p = kBase[index]; std::snprintf(name, nameSize, "%s", p.name); return true; }
    if (index >= kBaseCount + 16) { p = kRecordParams[index - kBaseCount - 16]; std::snprintf(name, nameSize, "%s", p.name); return true; }
    const int k = index - kBaseCount, slot = k % 8;
    const bool gate = k >= 8;
    p = {clap_id((gate ? pSlotGate1 : pSlotVel1) + slot), name, "Phrases", -100, 100, 0, nullptr};
    std::snprintf(name, nameSize, "Slot %d %s Offset", slot + 1, gate ? "Gate" : "Velocity");
    return true;
}
int paramIndex(clap_id id) { return id >= pSwitch && id < pEnd ? int(id - pSwitch) : -1; }

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_NOTE_EFFECT, nullptr};
const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION, "org.oscairn.arpeggiator", "Oscairn Arpeggiator", "Oscairn", "https://github.com/oscairn/arpeggiator",
    "", "", "0.1.0", "Phrase-based arpeggiator", kFeatures};

constexpr uint8_t kSysexId[] = {0xF0, 0x7D, 'O', 'P', 'H', 'R'};  // non-commercial ID, then "OPHR"
constexpr size_t kInboxSize = 1 << 18;

struct Plugin;

struct Out : arp::Sink {
    const clap_output_events_t* out = nullptr;
    bool push(const arp::Output& o) override {
        if (!out) return false;
        if (o.type == arp::Output::NoteOn || o.type == arp::Output::NoteOff) {
            clap_event_note_t e{};
            e.header = {sizeof e, o.frame, CLAP_CORE_EVENT_SPACE_ID, uint16_t(o.type == arp::Output::NoteOn ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
            e.note_id = o.id; e.port_index = 0; e.channel = o.channel; e.key = o.key;
            e.velocity = o.type == arp::Output::NoteOn ? o.velocity / 65535.0 : 0.0;
            return out->try_push(out, &e.header);
        }
        clap_event_midi_t m{};
        m.header = {sizeof m, o.frame, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
        m.port_index = 0;
        if (o.type == arp::Output::Control) { m.data[0] = uint8_t(0xB0 | o.channel); m.data[1] = o.key; m.data[2] = o.value; }
        else if (o.type == arp::Output::Bend) { const int v = o.bend + 8192; m.data[0] = uint8_t(0xE0 | o.channel); m.data[1] = uint8_t(v & 127); m.data[2] = uint8_t(v >> 7); }
        else { m.data[0] = uint8_t(0xD0 | o.channel); m.data[1] = o.value; }
        return out->try_push(out, &m.header);
    }
};

struct Plugin {
    clap_plugin_t api{};
    const clap_host_t* host;
    std::atomic<double> values[kParamCount];
    double rate = 48000;
    arp::Player player;
    Out sink;
    // Phrases: active belongs to the audio thread; pending and retired are hand-over points.
    arp::Phrase* active[arp::kSlots] = {};
    std::atomic<arp::Phrase*> pending[arp::kSlots] = {};
    std::atomic<arp::Phrase*> retired[arp::kSlots] = {};
    std::string text[arp::kSlots];  // main thread: the text form of each slot's phrase
    // A phrase pushed in on the note input waits here for the main thread to parse it.
    std::unique_ptr<uint8_t[]> inbox{new uint8_t[kInboxSize]};
    std::atomic<size_t> inboxSize{0};  // nonzero: full, owned by the main thread until cleared
    std::atomic<int> inboxSlot{0};
    bool playing = false, transportSeen = false;
    // Record mode: the audio thread captures while recordState is 1; 2 hands the notes to the main thread.
    arp::Recorder recorder;
    std::atomic<int> recordState{0};
    double recordClock = 0, recordTicksPerFrame = 0;

    static Plugin& of(const clap_plugin_t* p) { return *static_cast<Plugin*>(p->plugin_data); }

    explicit Plugin(const clap_host_t* h) : host(h) {
        for (int i = 0; i < kParamCount; ++i) {
            ParamInfo p; char n[64];
            paramInfo(i, p, n, sizeof n);
            values[i].store(p.def);
        }
        api.desc = &kDescriptor;
        api.plugin_data = this;
        api.init = [](const clap_plugin_t*) { return true; };
        api.destroy = [](const clap_plugin_t* p) { delete &of(p); };
        api.activate = [](const clap_plugin_t* p, double r, uint32_t, uint32_t) {
            if (!(r > 0) || !std::isfinite(r)) return false;
            of(p).rate = r;
            return true;
        };
        api.deactivate = [](const clap_plugin_t*) {};
        api.start_processing = [](const clap_plugin_t*) { return true; };
        api.stop_processing = [](const clap_plugin_t*) {};
        api.reset = [](const clap_plugin_t* p) { of(p).resetPending = true; };
        api.process = [](const clap_plugin_t* p, const clap_process_t* b) { return of(p).process(b); };
        api.get_extension = [](const clap_plugin_t*, const char* id) { return extension(id); };
        api.on_main_thread = [](const clap_plugin_t* p) { of(p).mainThread(); };
    }
    ~Plugin() {
        for (int s = 0; s < arp::kSlots; ++s) {
            delete active[s];
            delete pending[s].exchange(nullptr);
            delete retired[s].exchange(nullptr);
        }
    }
    std::atomic<bool> resetPending{false};

    int ival(clap_id id) const { return int(std::lround(values[paramIndex(id)].load(std::memory_order_relaxed))); }

    arp::Settings settings() const {
        arp::Settings s;
        s.on = ival(pSwitch) != 0 && recordState.load(std::memory_order_relaxed) != 1;
        s.hold = arp::HoldMode(std::clamp(ival(pHold), 0, 2));
        s.keyMode = arp::KeyMode(std::clamp(ival(pKeyMode), 0, 4));
        s.velocityMode = arp::VelocityMode(ival(pVelMode) != 0);
        s.trigger = arp::TriggerMode(ival(pTrigger) != 0);
        s.loop = ival(pLoop) != 0;
        s.playOnly = ival(pPlayOnly) != 0;
        s.unit = std::clamp(ival(pUnit), 0, 9);
        s.quantizeTicks = kGridTicks[std::clamp(ival(pQuantize), 0, 6)];
        s.quantizeStrength = ival(pStrength);
        s.swing = ival(pSwing);
        s.velocityRate = ival(pVelRate);
        s.gateRate = ival(pGateRate);
        s.octaveRange = ival(pOctRange);
        s.outputOctave = ival(pOutOct);
        s.noteLow = ival(pNoteLow); s.noteHigh = ival(pNoteHigh);
        s.velocityLow = ival(pVelLow); s.velocityHigh = ival(pVelHigh);
        s.changeTiming = arp::ChangeTiming(ival(pChange) != 0);
        s.accentThreshold = ival(pAccent);
        s.accentQuantize = ival(pAccentQ) != 0;
        s.randomSfx = ival(pSfx) != 0;
        s.sfxVelocityOffset = ival(pSfxVel);
        s.sfxKeyVelocity = ival(pSfxKey) != 0;
        s.fixedSdBd = ival(pSdBd) != 0;
        const int sync = std::clamp(ival(pSync), 0, 7);
        s.syncQuantize = sync ? kGridTicks[sync - 1] : 0;
        s.slot = std::clamp(ival(pSlot), 0, 7);
        for (int i = 0; i < 8; ++i) { s.slotVelocityOffset[i] = ival(clap_id(pSlotVel1 + i)); s.slotGateOffset[i] = ival(clap_id(pSlotGate1 + i)); }
        return s;
    }

    bool setParam(clap_id id, double v) {
        const int i = paramIndex(id);
        if (i < 0 || !std::isfinite(v)) return false;
        ParamInfo p; char n[64];
        paramInfo(i, p, n, sizeof n);
        values[i].store(std::clamp(std::round(v), p.min, p.max), std::memory_order_relaxed);
        return true;
    }

    void paramEvent(const clap_event_header_t* h) {
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE && h->size >= sizeof(clap_event_param_value_t)) {
            const auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
            setParam(e->param_id, e->value);
        }
    }

    // Main thread: parse text into a phrase and queue it for the audio thread.
    bool install(int slot, const char* data, size_t size) {
        if (slot < 0 || slot >= arp::kSlots) return false;
        auto ph = std::make_unique<arp::Phrase>();
        char err[160];
        if (!arp::parsePhrase(data, size, *ph, err, sizeof err)) {
            log(CLAP_LOG_WARNING, err);
            return false;
        }
        text[slot].assign(data, size);
        delete pending[slot].exchange(ph.release());
        return true;
    }

    void clearSlot(int slot) {
        text[slot].clear();
        delete pending[slot].exchange(nullptr);
        // An empty phrase (no tracks) stands for "none"; the audio thread swaps it in like any other.
        auto ph = new arp::Phrase();
        ph->trackCount = 0;
        pending[slot].store(ph);
    }

    void log(clap_log_severity sev, const char* msg) const {
        auto* l = host ? static_cast<const clap_host_log_t*>(host->get_extension(host, CLAP_EXT_LOG)) : nullptr;
        if (l && l->log) l->log(host, sev, msg);
    }

    void mainThread() {
        for (int s = 0; s < arp::kSlots; ++s) delete retired[s].exchange(nullptr);
        if (recordState.load(std::memory_order_acquire) == 2) {
            const std::string t = arp::recordedPhrase(recorder.notes(), recorder.count(), recorder.length(),
                                                      arp::RecordMode(std::clamp(ival(pRecordConvert), 0, 2)), "Recorded");
            if (recorder.count()) install(std::clamp(ival(pSlot), 0, 7), t.data(), t.size());
            recordState.store(0, std::memory_order_release);
        }
        if (const size_t n = inboxSize.load(std::memory_order_acquire)) {
            install(inboxSlot.load(std::memory_order_relaxed), reinterpret_cast<const char*>(inbox.get()), n);
            inboxSize.store(0, std::memory_order_release);
        }
    }

    // Audio thread: take queued phrases whose slot has no retired phrase still waiting to be freed.
    void takePhrases() {
        bool freed = false;
        for (int s = 0; s < arp::kSlots; ++s) {
            if (retired[s].load(std::memory_order_acquire) || !pending[s].load(std::memory_order_relaxed)) continue;
            arp::Phrase* p = pending[s].exchange(nullptr, std::memory_order_acq_rel);
            if (!p) continue;
            arp::Phrase* old = active[s];
            active[s] = p;
            player.setPhrase(s, p->trackCount ? p : nullptr);
            if (old) { retired[s].store(old, std::memory_order_release); freed = true; }
        }
        if (freed && host && host->request_callback) host->request_callback(host);
    }

    void sysex(const clap_event_midi_sysex_t& e) {
        // F0 7D 'O' 'P' 'H' 'R' slot <phrase text> F7
        if (e.port_index != 0 || !e.buffer || e.size < sizeof kSysexId + 2 || std::memcmp(e.buffer, kSysexId, sizeof kSysexId)) return;
        const size_t n = e.size - sizeof kSysexId - 1 - (e.buffer[e.size - 1] == 0xF7 ? 1 : 0);
        const int slot = e.buffer[sizeof kSysexId];
        if (slot >= arp::kSlots || n == 0 || n > kInboxSize || inboxSize.load(std::memory_order_acquire)) {
            return;  // shortcut: one phrase in flight; a second push before the first is installed is dropped
        }
        std::memcpy(inbox.get(), e.buffer + sizeof kSysexId + 1, n);
        inboxSlot.store(slot, std::memory_order_relaxed);
        inboxSize.store(n, std::memory_order_release);
        if (host && host->request_callback) host->request_callback(host);
    }

    void passMidi(const clap_event_midi_t& m, uint32_t frame, const clap_output_events_t* out) {
        clap_event_midi_t c = m;
        c.header.time = frame;
        c.port_index = 0;
        if (!out->try_push(out, &c.header)) player.panic(frame);
    }

    void noteEnd(const clap_event_note_t& in, uint32_t frame, const clap_output_events_t* out) {
        if (in.note_id < 0) return;
        clap_event_note_t e = in;
        e.header.type = CLAP_EVENT_NOTE_END;
        e.header.time = frame;
        out->try_push(out, &e.header);
    }

    void recover(uint32_t frame, const clap_output_events_t* out) {
        player.panic(frame);
        clap_event_note_t e{};
        e.header = {sizeof e, frame, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_CHOKE, 0};
        e.note_id = -1; e.port_index = 0; e.channel = -1; e.key = -1;
        out->try_push(out, &e.header);
    }

    void input(const clap_event_header_t* h, uint32_t frame, const clap_output_events_t* out) {
        if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
        switch (h->type) {
            case CLAP_EVENT_PARAM_VALUE: paramEvent(h); player.setSettings(settings()); return;
            case CLAP_EVENT_NOTE_ON:
            case CLAP_EVENT_NOTE_OFF:
            case CLAP_EVENT_NOTE_CHOKE: {
                if (h->size < sizeof(clap_event_note_t)) return;
                const auto& n = *reinterpret_cast<const clap_event_note_t*>(h);
                if (n.port_index > 0) return;
                if (h->type == CLAP_EVENT_NOTE_CHOKE && (n.key < 0 || n.channel < 0)) { recover(frame, out); return; }
                if (n.key < 0 || n.key > 127 || n.channel < 0 || n.channel > 15) return;
                if (h->type == CLAP_EVENT_NOTE_ON) {
                    const int v = std::clamp(int(std::lround((std::isfinite(n.velocity) ? n.velocity : 0) * 127)), 1, 127);
                    if (recordState.load(std::memory_order_relaxed) == 1) recorder.keyOn(recordClock + frame * recordTicksPerFrame, n.key, v);
                    player.keyOn(frame, n.channel, n.key, v, n.note_id);
                } else {
                    if (recordState.load(std::memory_order_relaxed) == 1) recorder.keyOff(recordClock + frame * recordTicksPerFrame, n.key);
                    player.keyOff(frame, n.channel, n.key, n.note_id);
                    noteEnd(n, frame, out);
                }
                return;
            }
            case CLAP_EVENT_MIDI: {
                if (h->size < sizeof(clap_event_midi_t)) return;
                const auto& m = *reinterpret_cast<const clap_event_midi_t*>(h);
                if (m.port_index != 0) return;
                const int kind = m.data[0] & 0xF0, ch = m.data[0] & 15;
                const bool rec = recordState.load(std::memory_order_relaxed) == 1;
                const double recTick = recordClock + frame * recordTicksPerFrame;
                if (kind == 0x90 && m.data[2]) { if (rec) recorder.keyOn(recTick, m.data[1] & 127, m.data[2] & 127); player.keyOn(frame, ch, m.data[1] & 127, m.data[2] & 127, -1); return; }
                if (kind == 0x80 || kind == 0x90) { if (rec) recorder.keyOff(recTick, m.data[1] & 127); player.keyOff(frame, ch, m.data[1] & 127, -1); return; }
                if (kind == 0xB0 && m.data[1] == 64 && player.settings().on) { player.sustain(frame, ch, m.data[2] >= 64); return; }
                if (kind == 0xB0 && (m.data[1] == 120 || m.data[1] == 123)) { player.advance(frame); recover(frame, out); }
                player.advance(frame);
                passMidi(m, frame, out);
                return;
            }
            case CLAP_EVENT_MIDI_SYSEX:
                if (h->size >= sizeof(clap_event_midi_sysex_t)) sysex(*reinterpret_cast<const clap_event_midi_sysex_t*>(h));
                return;
            default: return;
        }
    }

    clap_process_status process(const clap_process_t* b) {
        sink.out = b->out_events;
        if (resetPending.exchange(false)) recover(0, b->out_events);
        takePhrases();
        player.setSettings(settings());
        arp::Clock clock;
        clock.sampleRate = rate;
        if (b->transport) {
            const auto* t = b->transport;
            if (t->flags & CLAP_TRANSPORT_HAS_TEMPO) clock.tempo = t->tempo;
            const bool nowPlaying = t->flags & CLAP_TRANSPORT_IS_PLAYING;
            if (nowPlaying && (t->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE)) {
                clock.hasBeats = true;
                clock.beats = double(t->song_pos_beats) / CLAP_BEATTIME_FACTOR;
            }
            if (transportSeen && playing && !nowPlaying) recover(0, b->out_events);
            playing = nowPlaying;
            transportSeen = true;
        }
        recordTicksPerFrame = clock.tempo / 60.0 * 480.0 / rate;
        const bool wantRecord = ival(pRecord) != 0;
        if (wantRecord && recordState.load(std::memory_order_acquire) == 0) {
            recorder.arm(uint32_t(std::clamp(ival(pRecordBars), 1, 8)) * 1920u);
            recordState.store(1, std::memory_order_release);
        } else if (!wantRecord && recordState.load(std::memory_order_relaxed) == 1) {
            recorder.disarm();
            recordState.store(0, std::memory_order_release);
        }
        player.setSettings(settings());
        player.begin(clock, b->frames_count, sink);
        const uint32_t count = b->in_events ? b->in_events->size(b->in_events) : 0;
        for (uint32_t i = 0; i < count; ++i) {
            const clap_event_header_t* h = b->in_events->get(b->in_events, i);
            if (!h) continue;
            input(h, std::min(h->time, b->frames_count ? b->frames_count - 1 : 0), b->out_events);
            if (player.overflowed()) recover(std::min(h->time, b->frames_count), b->out_events);
        }
        player.end();
        if (player.overflowed()) recover(b->frames_count ? b->frames_count - 1 : 0, b->out_events);
        recordClock += recordTicksPerFrame * b->frames_count;
        if (recordState.load(std::memory_order_relaxed) == 1 && recorder.advance(recordClock)) {
            // Done: switch Record off, tell the host, and let the main thread make the phrase.
            values[paramIndex(pRecord)].store(0, std::memory_order_relaxed);
            clap_event_param_value_t pv{};
            pv.header = {sizeof pv, b->frames_count ? b->frames_count - 1 : 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
            pv.param_id = pRecord; pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1; pv.value = 0;
            b->out_events->try_push(b->out_events, &pv.header);
            recordState.store(2, std::memory_order_release);
            if (host && host->request_callback) host->request_callback(host);
        }
        sink.out = nullptr;
        return player.idle() ? CLAP_PROCESS_SLEEP : CLAP_PROCESS_CONTINUE;
    }

    // State: a text header, then parameters, then each slot's phrase text with its byte count.
    bool save(const clap_ostream_t* s) const {
        std::string out = "oscairn-arpeggiator 1\n";
        char line[96];
        for (int i = 0; i < kParamCount; ++i) {
            ParamInfo p; char n[64];
            paramInfo(i, p, n, sizeof n);
            std::snprintf(line, sizeof line, "param %u %d\n", p.id, int(std::lround(values[i].load())));
            out += line;
        }
        for (int sl = 0; sl < arp::kSlots; ++sl) {
            if (text[sl].empty()) continue;
            std::snprintf(line, sizeof line, "slot %d %zu\n", sl, text[sl].size());
            out += line;
            out += text[sl];
            out += '\n';
        }
        size_t done = 0;
        while (done < out.size()) {
            const int64_t w = s->write(s, out.data() + done, out.size() - done);
            if (w <= 0) return false;
            done += size_t(w);
        }
        return true;
    }

    bool load(const clap_istream_t* s) {
        std::string in;
        char buf[4096];
        for (;;) {
            const int64_t r = s->read(s, buf, sizeof buf);
            if (r < 0) return false;
            if (r == 0) break;
            in.append(buf, size_t(r));
            if (in.size() > (64u << 20)) return false;
        }
        if (in.compare(0, 22, "oscairn-arpeggiator 1\n")) return false;
        size_t pos = 22;
        bool seen[arp::kSlots] = {};
        while (pos < in.size()) {
            const size_t eol = in.find('\n', pos);
            if (eol == std::string::npos) return false;
            const std::string line = in.substr(pos, eol - pos);
            pos = eol + 1;
            unsigned id; int v; int slot; size_t len;
            if (std::sscanf(line.c_str(), "param %u %d", &id, &v) == 2) setParam(id, v);
            else if (std::sscanf(line.c_str(), "slot %d %zu", &slot, &len) == 2) {
                if (slot < 0 || slot >= arp::kSlots || len > in.size() - pos) return false;
                if (!install(slot, in.data() + pos, len)) return false;
                seen[slot] = true;
                pos += len + 1;
            } else if (!line.empty()) return false;
        }
        for (int sl = 0; sl < arp::kSlots; ++sl) if (!seen[sl] && !text[sl].empty()) clearSlot(sl);
        return true;
    }

    static const void* extension(const char* id);
};

const clap_plugin_audio_ports_t kAudioPorts = {
    [](const clap_plugin_t*, bool) -> uint32_t { return 0; },
    [](const clap_plugin_t*, uint32_t, bool, clap_audio_port_info_t*) { return false; }};

const clap_plugin_note_ports_t kNotePorts = {
    [](const clap_plugin_t*, bool) -> uint32_t { return 1; },
    [](const clap_plugin_t*, uint32_t i, bool in, clap_note_port_info_t* info) {
        if (i) return false;
        *info = {};
        info->id = 0;
        info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        std::snprintf(info->name, sizeof info->name, "%s", in ? "Notes In" : "Notes Out");
        return true;
    }};

const clap_plugin_params_t kParams = {
    [](const clap_plugin_t*) -> uint32_t { return kParamCount; },
    [](const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
        ParamInfo p; char n[64];
        if (!paramInfo(int(index), p, n, sizeof n)) return false;
        *info = {};
        info->id = p.id;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED | (p.texts ? CLAP_PARAM_IS_ENUM : 0);
        std::snprintf(info->name, sizeof info->name, "%s", n);
        std::snprintf(info->module, sizeof info->module, "%s", p.module);
        info->min_value = p.min; info->max_value = p.max; info->default_value = p.def;
        return true;
    },
    [](const clap_plugin_t* pl, clap_id id, double* v) {
        const int i = paramIndex(id);
        if (i < 0) return false;
        *v = Plugin::of(pl).values[i].load();
        return true;
    },
    [](const clap_plugin_t*, clap_id id, double v, char* out, uint32_t size) {
        const int i = paramIndex(id);
        ParamInfo p; char n[64];
        if (i < 0 || !paramInfo(i, p, n, sizeof n)) return false;
        const int k = int(std::lround(v));
        if (p.texts && k >= p.min && k <= p.max) std::snprintf(out, size, "%s", p.texts[k - int(p.min)]);
        else if (id == pSlot) std::snprintf(out, size, "%d", k + 1);
        else if (id == pAccent && k == 0) std::snprintf(out, size, "Off");
        else if (id == pVelRate || id == pGateRate || id == pStrength || (id >= pSlotVel1 && id < pRecord)) std::snprintf(out, size, "%d%%", k);
        else if (id == pNoteLow || id == pNoteHigh) {
            static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            std::snprintf(out, size, "%s%d", names[k % 12], k / 12 - 2);
        } else std::snprintf(out, size, "%d", k);
        return true;
    },
    [](const clap_plugin_t*, clap_id id, const char* text, double* v) {
        if (paramIndex(id) < 0 || !text) return false;
        char* end = nullptr;
        const double x = std::strtod(text, &end);
        if (end == text) return false;
        *v = id == pSlot ? x - 1 : x;
        return true;
    },
    [](const clap_plugin_t* pl, const clap_input_events_t* in, const clap_output_events_t*) {
        auto& p = Plugin::of(pl);
        for (uint32_t i = 0; in && i < in->size(in); ++i) if (const auto* h = in->get(in, i)) p.paramEvent(h);
    }};

const clap_plugin_state_t kState = {
    [](const clap_plugin_t* p, const clap_ostream_t* s) { return Plugin::of(p).save(s); },
    [](const clap_plugin_t* p, const clap_istream_t* s) { return Plugin::of(p).load(s); }};

// Loading a phrase file through the host's preset-load puts it in the selected slot.
const clap_plugin_preset_load_t kPresetLoad = {
    [](const clap_plugin_t* pl, uint32_t kind, const char* location, const char*) {
        auto& p = Plugin::of(pl);
        if (kind != CLAP_PRESET_DISCOVERY_LOCATION_FILE || !location) return false;
        FILE* f = std::fopen(location, "rb");
        if (!f) return false;
        std::string data;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && data.size() < (16u << 20)) data.append(buf, n);
        std::fclose(f);
        return p.install(std::clamp(p.ival(pSlot), 0, 7), data.data(), data.size());
    }};

const void* Plugin::extension(const char* id) {
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kAudioPorts;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &kNotePorts;
    if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &kParams;
    if (!std::strcmp(id, CLAP_EXT_STATE)) return &kState;
    if (!std::strcmp(id, CLAP_EXT_PRESET_LOAD)) return &kPresetLoad;
    return nullptr;
}

const clap_plugin_factory_t kFactory = {
    [](const clap_plugin_factory_t*) -> uint32_t { return 1; },
    [](const clap_plugin_factory_t*, uint32_t i) -> const clap_plugin_descriptor_t* { return i ? nullptr : &kDescriptor; },
    [](const clap_plugin_factory_t*, const clap_host_t* h, const char* id) -> const clap_plugin_t* {
        if (std::strcmp(id, kDescriptor.id)) return nullptr;
        return &(new Plugin(h))->api;
    }};

}  // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION, [](const char*) { return true; }, []() {},
    [](const char* id) -> const void* { return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &kFactory : nullptr; }};
