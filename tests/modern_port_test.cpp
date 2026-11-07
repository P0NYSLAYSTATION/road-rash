#include "presentation.hpp"
#include "native_controllers.hpp"
#include "input.hpp"
#include "settings.hpp"
#include "net/direct_tcp.hpp"
#include "net/mano.hpp"
#include "net/compat.hpp"
#include "assets.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace rr;
void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
template <class Poll, class Done> void until(Poll poll, Done done, int timeout = 6000) {
    const auto end = net::steady_ms() + timeout;
    while (!done() && net::steady_ms() < end) {
        poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(done(), "network operation timed out");
}
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "controllers";
        if (mode == "controllers") {
            auto sample = xinput_sample(0x1101, 16384, 32767, 255, 0);
            require(sample.buttons[SDL_GAMEPAD_BUTTON_SOUTH] &&
                        sample.buttons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] &&
                        sample.buttons[SDL_GAMEPAD_BUTTON_DPAD_UP],
                    "XInput button mapping");
            require(sample.axes[SDL_GAMEPAD_AXIS_LEFTY] == -32767, "XInput Y orientation");
            require(sample.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] == 32767 &&
                        sample.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] == 0,
                    "independent XInput triggers");
            sample = xinput_sample(0, 0, -32768, 0, 255);
            require(sample.axes[SDL_GAMEPAD_AXIS_LEFTY] == 32767 &&
                        sample.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] == 0 &&
                        sample.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] == 32767,
                    "trigger independence and negative extreme");
            uint8_t buttons[128]{};
            buttons[0] = 128;
            sample = directinput_sample(-32768, 32767, 4500, buttons);
            require(sample.buttons[SDL_GAMEPAD_BUTTON_SOUTH] && sample.buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] &&
                        sample.buttons[SDL_GAMEPAD_BUTTON_DPAD_RIGHT],
                    "DirectInput buttons and diagonal POV");
            require(!sample.buttons[SDL_GAMEPAD_BUTTON_EAST] &&
                        !sample.buttons[SDL_GAMEPAD_BUTTON_DPAD_DOWN] &&
                        sample.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] == 0,
                    "DirectInput does not invent throttle/button presses");
            PlayerInput intent;
            intent.nitro = true;
            require(controls_from(intent) == control_nitro, "controller boost reaches simulation controls");
            auto bindings = modern_keyboard_controls();
            bool keys[SDL_SCANCODE_COUNT]{};
            constexpr uint32_t expected[] = {control_left, control_right, control_lean_down, control_lean_up};
            for (int i = 0; i < 4; ++i) {
                const int action = binding_actions[size_t(i + 2)];
                keys[bindings[size_t(action)]] = true;
                require(bound_control_bits(bindings, true, keys) == expected[i],
                        "turn and lean bindings must emit distinct simulation bits");
                keys[bindings[size_t(action)]] = false;
            }
            Settings settings;
            settings.custom_controls = true;
            settings.controls = bindings;
            settings.controls[10] = SDL_SCANCODE_KP_7;
            settings.controller_bindings[4] = 1005;
            settings.controller_bindings[5] = 149;
            const auto restored = parse_settings(format_settings(settings));
            require(restored.controls == settings.controls &&
                        restored.controller_bindings == settings.controller_bindings,
                    "keyboard and controller bindings persist exactly");
            SDL_Event event{};
            event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            event.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHTX;
            event.gaxis.value = -24000;
            require(controller_capture_binding(event) == 1005, "lean axis capture");
            event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            event.gbutton.button = 149;
            require(controller_capture_binding(event) == 150, "DirectInput high button capture");
            std::cout << "XInput/DirectInput axis, trigger, POV, button and boost checks passed\n";
            return 0;
        }
        if (mode == "scaling") {
            SDL_setenv_unsafe("SDL_VIDEODRIVER", "windows", 1);
            SDL_setenv_unsafe("RR_RENDERER", "dx9", 1);
            require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            require(create_game_window_renderer("Scaling validation", 1000, 700, SDL_WINDOW_HIDDEN, &window,
                                                &renderer),
                    SDL_GetError());
            require(!(SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD),
                    "SDL controller subsystem was initialized");
            SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, nullptr);
            SurfacePtr captured(read_presentation_frame(renderer));
            require(bool(captured), SDL_GetError());
            require(captured->w == 1000 && captured->h == 700,
                    "presentation capture discarded integer borders");
            SurfacePtr argb(SDL_ConvertSurface(captured.get(), SDL_PIXELFORMAT_ARGB8888));
            auto pixel = [&](int x, int y) {
                return reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(argb->pixels) + y * argb->pitch)[x] &
                       0xffffff;
            };
            require(pixel(0, 0) == 0 && pixel(179, 109) == 0 && pixel(180, 110) == 0xff0000 &&
                        pixel(819, 589) == 0xff0000 && pixel(820, 590) == 0,
                    "integer image is not centered at exact 1x scale");
            require(present_frame(renderer), "DirectX 9 integer presentation");
            int w, h;
            SDL_RendererLogicalPresentation mode;
            SDL_GetRenderLogicalPresentation(renderer, &w, &h, &mode);
            require(w == 640 && h == 480 && mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE,
                    "capture changed renderer logical state");
            presentation_low_resolution(renderer, true);
            require(present_frame(renderer), "320x240 framebuffer presentation");
            require(SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), "rr.framebuffer.width", 0) ==
                            320 &&
                        SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), "rr.framebuffer.height",
                                              0) == 240,
                    "low-resolution framebuffer is not native 320x240");
            auto rect = presentation_rectangle(1000, 700, 320, 240, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
            float x = 0, y = 0;
            presentation_coordinates_from_window(renderer, rect.x, rect.y, &x, &y);
            require(std::abs(x) < .01 && std::abs(y) < .01,
                    "mouse mapping does not match native integer framebuffer");
            destroy_game_renderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            std::cout << "Exact integer borders, logical state and 320x240 framebuffer checks passed\n";
            return 0;
        }
        if (mode == "data-compat") {
            require(argc >= 4, "two asset roots required");
            Assets first(argv[2]), second(argv[3]);
            require(
                net::gameplay_data_digest(first) == net::gameplay_data_digest(second),
                "PC compatibility patches or optional audio/video incorrectly changed the gameplay digest");
            std::cout << "Patched installation and original CD share identical gameplay tables/assets\n";
            return 0;
        }
        if (mode == "network") {
            using namespace rr::net;
            const auto port = direct_start_host(0);
            const auto address = "tcp://127.0.0.1:" + std::to_string(port);
            Hello hello;
            hello.sim_fingerprint = 0x1234;
            hello.alias = "Host";
            RoomClient a([&] { return direct_connect(address); }, hello);
            hello.alias = "Guest";
            RoomClient b([&] { return direct_connect(address); }, hello);
            auto pump = [&] {
                const auto now = steady_ms();
                a.update(now);
                b.update(now);
            };
            a.create_room(RoomOptions{0, 0, false, true});
            until(pump, [&] { return a.in_room(); });
            const auto code = a.room()->code;
            direct_refresh_discovery(port);
            until(pump, [&] {
                auto rooms = direct_discovered_rooms();
                return std::any_of(rooms.begin(), rooms.end(),
                                   [&](const auto& room) { return room.code == code; });
            });
            require(!direct_room_endpoint(code).empty(), "discovery did not retain host address");
            auto invite = direct_parse_invite(direct_invite(code));
            require(invite && invite->second == code, "direct invite round trip");
            b.join_room_text(code);
            until(pump, [&] { return b.in_room() && a.occupied_count() == 2; });
            a.pick_character(1);
            b.pick_character(2);
            a.send_chat_text("test");
            bool chat = false;
            until(
                [&] {
                    pump();
                    for (const auto& line : b.take_chat()) chat |= line.text == "test";
                },
                [&] { return chat; });
            a.vote_course(4);
            b.vote_course(4);
            std::optional<Start> start_a, start_b;
            until(
                [&] {
                    pump();
                    if (auto s = a.take_start()) start_a = *s;
                    if (auto s = b.take_start()) start_b = *s;
                },
                [&] { return start_a && start_b; });
            require(start_a->humans.size() == 2 && start_a->seed == start_b->seed &&
                        start_a->race_id == start_b->race_id,
                    "peers received different race setup");
            auto ca = a.release_channel(), cb = b.release_channel();
            ca->send(encode(Input{0, {1, 1, 1, 1}}));
            cb->send(encode(Input{0, {2, 2, 2, 2}}));
            bool sealed_a = false, sealed_b = false;
            auto receive = [&](Channel& channel, bool& found) {
                while (auto bytes = channel.poll())
                    if (auto message = decode(*bytes)) {
                        if (auto p = std::get_if<Ping>(&*message)) channel.send(encode(Pong{p->t, 0, 0}));
                        if (auto f = std::get_if<Frames>(&*message);
                            f && f->first_tick == 0 && !f->frames.empty()) {
                            require(f->humans == 2 && f->frames[0].words.size() == 2,
                                    "invalid sealed input frame");
                            require(f->frames[0].words[size_t(start_a->your_seat)] == 1 &&
                                        f->frames[0].words[size_t(start_b->your_seat)] == 2,
                                    "TCP race inputs were lost or reordered");
                            found = true;
                        }
                    }
            };
            until(
                [&] {
                    receive(*ca, sealed_a);
                    receive(*cb, sealed_b);
                },
                [&] { return sealed_a && sealed_b; }, 8000);
            ca->close();
            cb->close();
            direct_stop_host();
            std::cout
                << "Real TCP host/join, UDP discovery, invite, chat, race setup and sealed inputs passed\n";
            return 0;
        }
        return 2;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
