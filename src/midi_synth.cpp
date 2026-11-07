#include "midi_synth.hpp"
#include "utf8_path.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace rr {
std::vector<std::string> soundfont_candidates(const std::string& installation) {
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    auto scan = [&](const fs::path& dir) {
        std::error_code ec;
        std::vector<fs::path> found;
        for (fs::directory_iterator i(dir, ec), end; !ec && i != end; i.increment(ec)) {
            auto ext = path_to_utf8(i->path().extension());
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (i->is_regular_file(ec) && (ext == ".sf2" || ext == ".sf3" || ext == ".dls"))
                found.push_back(i->path());
        }
        // total-order: full filesystem path gives deterministic font priority.
        std::sort(found.begin(), found.end());
        for (const auto& p : found) out.push_back(path_to_utf8(p));
    };
    if (!installation.empty()) {
        scan(path_from_utf8(installation));
        scan(path_from_utf8(installation) / "soundfonts");
    }
    const auto exe = path_from_utf8(SDL_GetBasePath() ? SDL_GetBasePath() : ".");
    scan(exe);
    scan(exe / "soundfonts");
#ifdef _WIN32
    wchar_t sys[MAX_PATH];
    const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n && n < MAX_PATH) out.push_back(path_to_utf8(fs::path(sys) / L"drivers" / L"gm.dls"));
#endif
    return out;
}
struct MidiSynth::Impl {
    SDL_SharedObject* library = nullptr;
    void* settings = nullptr;
    void* synth = nullptr;
    std::string font;
    void* (*create_settings)() = nullptr;
    void (*delete_settings)(void*) = nullptr;
    int (*setnum)(void*, const char*, double) = nullptr;
    int (*setint)(void*, const char*, int) = nullptr;
    void* (*create_synth)(void*) = nullptr;
    void (*delete_synth)(void*) = nullptr;
    int (*load)(void*, const char*, int) = nullptr;
    int (*noteon)(void*, int, int, int) = nullptr;
    int (*noteoff)(void*, int, int) = nullptr;
    int (*program)(void*, int, int) = nullptr;
    int (*cc)(void*, int, int, int) = nullptr;
    int (*bend)(void*, int, int) = nullptr;
    int (*pressure)(void*, int, int) = nullptr;
    int (*key_pressure)(void*, int, int, int) = nullptr;
    int (*reset)(void*) = nullptr;
    int (*write)(void*, int, void*, int, int, void*, int, int) = nullptr;
    template <class T> void symbol(T& p, const char* name) {
        p = reinterpret_cast<T>(SDL_LoadFunction(library, name));
        if (!p) throw std::runtime_error(std::string("Missing FluidSynth symbol: ") + name);
    }
    ~Impl() {
        if (synth && delete_synth) delete_synth(synth);
        if (settings && delete_settings) delete_settings(settings);
        if (library) SDL_UnloadObject(library);
    }
};
MidiSynth::MidiSynth(const std::string& installation) : impl_(std::make_unique<Impl>()) {
    auto& p = *impl_;
    const std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
#ifdef _WIN32
    for (const char* name : {"libfluidsynth-3.dll", "fluidsynth.dll"}) {
        p.library = SDL_LoadObject((base + name).c_str());
        if (p.library) break;
    }
#else
    for (const char* name : {"libfluidsynth.so.3", "libfluidsynth.3.dylib"}) {
        p.library = SDL_LoadObject(name);
        if (p.library) break;
    }
#endif
    if (!p.library)
        throw std::runtime_error(std::string("Built-in MIDI: FluidSynth runtime unavailable: ") +
                                 SDL_GetError());
    p.symbol(p.create_settings, "new_fluid_settings");
    p.symbol(p.delete_settings, "delete_fluid_settings");
    p.symbol(p.setnum, "fluid_settings_setnum");
    p.symbol(p.setint, "fluid_settings_setint");
    p.symbol(p.create_synth, "new_fluid_synth");
    p.symbol(p.delete_synth, "delete_fluid_synth");
    p.symbol(p.load, "fluid_synth_sfload");
    p.symbol(p.noteon, "fluid_synth_noteon");
    p.symbol(p.noteoff, "fluid_synth_noteoff");
    p.symbol(p.program, "fluid_synth_program_change");
    p.symbol(p.cc, "fluid_synth_cc");
    p.symbol(p.bend, "fluid_synth_pitch_bend");
    p.symbol(p.pressure, "fluid_synth_channel_pressure");
    p.symbol(p.key_pressure, "fluid_synth_key_pressure");
    p.symbol(p.reset, "fluid_synth_system_reset");
    p.symbol(p.write, "fluid_synth_write_float");
    p.settings = p.create_settings();
    if (!p.settings) throw std::runtime_error("FluidSynth settings allocation failed");
    p.setnum(p.settings, "synth.sample-rate", 44100.0);
    p.setnum(p.settings, "synth.gain", .2);
    p.setint(p.settings, "synth.polyphony", 256);
    p.setint(p.settings, "synth.reverb.active", 1);
    p.setint(p.settings, "synth.chorus.active", 1);
    p.synth = p.create_synth(p.settings);
    if (!p.synth) throw std::runtime_error("FluidSynth synthesizer allocation failed");
    for (const auto& file : soundfont_candidates(installation)) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path_from_utf8(file), ec)) continue;
        if (p.load(p.synth, file.c_str(), 1) >= 0) {
            p.font = file;
            break;
        }
    }
    if (p.font.empty())
        throw std::runtime_error(
            "No playable SF2/SF3/DLS soundfont found in the installation or Windows gm.dls");
    SDL_Log("Built-in MIDI soundfont: %s", p.font.c_str());
}
MidiSynth::~MidiSynth() = default;
const std::string& MidiSynth::soundfont() const {
    return impl_->font;
}
void MidiSynth::reset() {
    impl_->reset(impl_->synth);
}
void MidiSynth::event(unsigned char status, unsigned char a, unsigned char b) {
    auto& p = *impl_;
    const int ch = status & 15;
    switch (status & 240) {
    case 128:
        p.noteoff(p.synth, ch, a);
        break;
    case 144:
        if (b)
            p.noteon(p.synth, ch, a, b);
        else
            p.noteoff(p.synth, ch, a);
        break;
    case 160:
        p.key_pressure(p.synth, ch, a, b);
        break;
    case 176:
        p.cc(p.synth, ch, a, b);
        break;
    case 192:
        p.program(p.synth, ch, a);
        break;
    case 208:
        p.pressure(p.synth, ch, a);
        break;
    case 224:
        p.bend(p.synth, ch, int(a) | (int(b) << 7));
        break;
    default:
        break;
    }
}
void MidiSynth::render(float* left, float* right, int frames) {
    impl_->write(impl_->synth, frames, left, 0, 1, right, 0, 1);
}
} // namespace rr
