#include "mixer.hpp"
#include "race_music.hpp"
#include "assets.hpp"
#include "audio_3do_math.hpp"
#include "midi_synth.hpp"
#include "presentation.hpp"
#include "wav.hpp"
#include "utf8_path.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
using namespace rr;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "mixer";
        if (mode == "mixer") {
            check(angle_3do(1, 0) == 0 && angle_3do(0, 1) == 0x400000 && angle_3do(-1, 0) == 0x800000 &&
                      angle_3do(0, -1) == 0xc00000,
                  "3DO angle quadrants wrong");
            check(angle_3do(1, 1) == 0x200000, "3DO diagonal angle wrong");
            Mixer mixer(8);
            PcmSound source{44100, 1, {1000, 2000, 3000, 9999}};
            source.loop_begin = 1;
            source.loop_end = 3;
            int voice = mixer.play_3do(&source, 32767, 64, 32768, true);
            int16_t out[12]{};
            mixer.render(out, 6);
            const int expected[] = {1000, 2000, 3000, 2000, 3000, 2000};
            for (int i = 0; i < 6; ++i) {
                check(out[2 * i] == expected[i], "native loop endpoint or pitch law wrong");
                check(out[2 * i + 1] == expected[i], "center panning wrong");
            }
            mixer.stop(voice);
            source.loop_begin = 0;
            source.loop_end = 0;
            voice = mixer.play_3do(&source, 64, 64, 32768, true);
            mixer.render(out, 1);
            check(out[0] > 0, "quiet native voice rounded to silence");
            mixer.set_3do(voice, 32767, 0, 0);
            mixer.render(out, 1);
            check(out[0] > 0 && out[1] == 0, "3DO linear hard-left pan wrong");
            mixer.stop(voice);
            mixer.play_3do(&source, 32767, 64, 16384, true);
            mixer.render(out, 3);
            check(out[0] == 1000 && out[2] == 1500 && out[4] == 2000, "native half-rate interpolation wrong");
            std::cout << "Native loop, raw frequency, full-precision gain and panning passed\n";
            return 0;
        }
        if (mode == "midi") {
#ifdef _WIN32
            MidiSynth fallback("__missing_font_directory__");
            check(fallback.soundfont().find("gm.dls") != std::string::npos,
                  "Windows soundfont fallback not selected");
            float left[8192]{}, right[8192]{};
            fallback.event(0x90, 69, 120);
            fallback.render(left, right, 8192);
            double energy = 0;
            for (float x : left) energy += x * x;
            check(energy > 1e-7, "built-in MIDI rendered silence");
            const auto temp = std::filesystem::temp_directory_path() / "rr-midi-local-bank-test";
            std::filesystem::create_directories(temp);
            std::filesystem::copy_file(path_from_utf8(fallback.soundfont()), temp / "local.dls",
                                       std::filesystem::copy_options::overwrite_existing);
            {
                MidiSynth local(path_to_utf8(temp));
                check(local.soundfont().find("local.dls") != std::string::npos,
                      "local soundfont did not take priority");
            }
            std::filesystem::remove(temp / "local.dls");
            std::filesystem::remove(temp);
            std::cout << "Built-in MIDI rendered audio; local bank priority and Windows fallback passed\n";
            return 0;
#else
            return 77;
#endif
        }
        if (mode == "race-midi") {
            if (argc < 3 || !std::filesystem::exists(path_from_utf8(argv[2]) / "AUDIO/MUSIC/GRUNGE1.MID"))
                return 77;
            Assets assets(argv[2]);
            Mixer mixer(8);
            RaceMusicPlayer player(assets, mixer);
            for (int course = 1; course <= 6; ++course) {
                player.play_race(std::min(course, 5), course == 6 ? "bozo" : "");
                check(player.active() && player.event_count() > 0, "race MIDI could not start");
                int64_t energy = 0;
                int16_t pcm[1470];
                for (int tick = 0; tick < 240; ++tick) {
                    player.update();
                    mixer.render(pcm, 735);
                    for (auto x : pcm) energy += int64_t(x) * x;
                }
                check(energy > 100000, "race sequencer rendered silence");
                player.stop();
                mixer.render(pcm, 1);
            }
            std::cout << "All five race songs and Klown rendered through the built-in MIDI player\n";
            return 0;
        }
        if (mode == "dx9") {
#ifdef _WIN32
            SDL_setenv_unsafe("RR_RENDERER", "dx9", 1);
            SDL_setenv_unsafe("SDL_VIDEODRIVER", "windows", 1);
            check(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            check(create_game_window_renderer("DirectX 9 validation", 64, 64, SDL_WINDOW_HIDDEN, &window,
                                              &renderer),
                  SDL_GetError());
            check(uses_directx9(renderer), "DirectX 9 backend not selected");
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderClear(renderer);
            check(present_frame(renderer), "DirectX 9 present failed");
            SDL_SetWindowSize(window, 96, 72);
            SDL_PumpEvents();
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_RenderClear(renderer);
            check(present_frame(renderer), "DirectX 9 resize/reset failed");
            auto hwnd = static_cast<HWND>(SDL_GetPointerProperty(
                SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
            check(hwnd && GetWindow(hwnd, GW_CHILD) == nullptr, "child window created");
            destroy_game_renderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            std::cout << "DirectX 9 present, resize and no-child-window checks passed\n";
            return 0;
#else
            return 77;
#endif
        }
        return 2;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
