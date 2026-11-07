#pragma once
#include "sdl_handles.hpp"
#include <array>
#include <cstdint>
namespace rr {
struct ControllerSample {
    std::array<bool, 160> buttons{};
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> axes{};
};
ControllerSample xinput_sample(uint16_t buttons, Sint16 x, Sint16 y, uint8_t left, uint8_t right,
                               Sint16 rx = 0, Sint16 ry = 0);
ControllerSample directinput_sample(int x, int y, uint32_t pov, const uint8_t* buttons);
void controller_set_window(SDL_Window*);
void controller_poll();
GamepadPtr controller_first();
GamepadPtr controller_open(SDL_JoystickID);
bool controller_connected(ControllerDevice*);
SDL_JoystickID controller_id(ControllerDevice*);
bool controller_button(ControllerDevice*, int);
Sint16 controller_axis(ControllerDevice*, SDL_GamepadAxis);
bool controller_xinput(ControllerDevice*);
} // namespace rr
