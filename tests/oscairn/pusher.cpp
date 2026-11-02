// SPDX-License-Identifier: MIT
// A note effect for the hosted test: passes its input on and, 1000 samples after it starts, pushes a
// phrase into slot 0 of whatever follows (F0 7D "OPHR" 00 <text> F7).
#include <clap/clap.h>
#include <cstring>
#include <string>

namespace {
const char* kFeatures[] = {CLAP_PLUGIN_FEATURE_NOTE_EFFECT, nullptr};
const clap_plugin_descriptor_t kDesc{CLAP_VERSION, "org.oscairn.arpeggiator.test-pusher", "Phrase pusher", "Oscairn", "", "", "", "1", "", kFeatures};
const char* kDown = "oscairn-phrase 1\nname \"down\"\nlength 480\ntrack\n  select down\n  note 0 p0 v100 g60\n  note 120 p0 v100 g60\n"
                    "  note 240 p0 v100 g60\n  note 360 p0 v100 g60\nend\n";
struct Plugin {
    clap_plugin_t api{};
    long position = 0;
    bool sent = false;
    std::string message;
    Plugin() {
        message = std::string("\xF0\x7D" "OPHR") + char(0) + kDown + "\xF7";
        api.desc = &kDesc; api.plugin_data = this;
        api.init = [](const clap_plugin_t*) { return true; };
        api.destroy = [](const clap_plugin_t* p) { delete static_cast<Plugin*>(p->plugin_data); };
        api.activate = [](const clap_plugin_t*, double, uint32_t, uint32_t) { return true; };
        api.deactivate = [](const clap_plugin_t*) {};
        api.start_processing = [](const clap_plugin_t*) { return true; };
        api.stop_processing = [](const clap_plugin_t*) {};
        api.reset = [](const clap_plugin_t*) {};
        api.on_main_thread = [](const clap_plugin_t*) {};
        api.process = [](const clap_plugin_t* p, const clap_process_t* b) -> clap_process_status {
            auto& s = *static_cast<Plugin*>(p->plugin_data);
            const uint32_t n = b->in_events->size(b->in_events);
            bool pushed = s.sent;
            auto push = [&](uint32_t at) {
                clap_event_midi_sysex_t e{};
                e.header = {sizeof e, at, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI_SYSEX, 0};
                e.port_index = 0; e.buffer = reinterpret_cast<const uint8_t*>(s.message.data()); e.size = uint32_t(s.message.size());
                b->out_events->try_push(b->out_events, &e.header);
                pushed = true;
            };
            const long due = 1000 - s.position;
            for (uint32_t i = 0; i < n; ++i) {
                const auto* h = b->in_events->get(b->in_events, i);
                if (!pushed && due >= 0 && long(h->time) > due) push(uint32_t(due));
                if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type != CLAP_EVENT_TRANSPORT) b->out_events->try_push(b->out_events, h);
            }
            if (!pushed && due >= 0 && due < long(b->frames_count)) push(uint32_t(due));
            s.sent = pushed;
            s.position += b->frames_count;
            return CLAP_PROCESS_CONTINUE;
        };
        api.get_extension = [](const clap_plugin_t*, const char* id) -> const void* {
            static const clap_plugin_note_ports_t ports{
                [](const clap_plugin_t*, bool) -> uint32_t { return 1; },
                [](const clap_plugin_t*, uint32_t i, bool, clap_note_port_info_t* info) {
                    if (i) return false;
                    *info = {}; info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
                    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP; std::strcpy(info->name, "Notes");
                    return true;
                }};
            return std::strcmp(id, CLAP_EXT_NOTE_PORTS) ? nullptr : &ports;
        };
    }
};
const clap_plugin_factory_t kFactory{[](const clap_plugin_factory_t*) -> uint32_t { return 1; },
                                     [](const clap_plugin_factory_t*, uint32_t i) { return i ? nullptr : &kDesc; },
                                     [](const clap_plugin_factory_t*, const clap_host_t*, const char* id) -> const clap_plugin_t* {
                                         return std::strcmp(id, kDesc.id) ? nullptr : &(new Plugin)->api;
                                     }};
}  // namespace
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{
    CLAP_VERSION, [](const char*) { return true; }, []() {},
    [](const char* id) -> const void* { return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &kFactory; }};
