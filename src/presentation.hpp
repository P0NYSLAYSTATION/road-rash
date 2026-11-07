#pragma once
#include <SDL3/SDL.h>
namespace rr {
bool create_game_window_renderer(const char*, int, int, SDL_WindowFlags, SDL_Window**, SDL_Renderer**);
bool present_frame(SDL_Renderer*);
void destroy_game_renderer(SDL_Renderer*);
void presentation_vsync(SDL_Renderer*, int);
bool uses_directx9(SDL_Renderer*);
} // namespace rr
