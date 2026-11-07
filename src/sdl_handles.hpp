#pragma once
#include <SDL3/SDL.h>
#include <memory>
namespace rr {
struct SdlTextureDeleter {
    void operator()(SDL_Texture* t) const noexcept { SDL_DestroyTexture(t); }
};
#ifdef _WIN32
struct NativeController {
    SDL_JoystickID id = 0;
};
using ControllerDevice = NativeController;
struct SdlGamepadDeleter {
    void operator()(NativeController* p) const noexcept { delete p; }
};
#else
using ControllerDevice = SDL_Gamepad;
struct SdlGamepadDeleter {
    void operator()(SDL_Gamepad* p) const noexcept { SDL_CloseGamepad(p); }
};
#endif
struct SdlSurfaceDeleter {
    void operator()(SDL_Surface* s) const noexcept { SDL_DestroySurface(s); }
};
using TexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;
using GamepadPtr = std::unique_ptr<ControllerDevice, SdlGamepadDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SdlSurfaceDeleter>;
} // namespace rr
