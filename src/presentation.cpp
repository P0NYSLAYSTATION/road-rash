#include "runtime_timing.hpp"
#include "presentation.hpp"
#include "port_config.hpp"
#include "native_controllers.hpp"
#include <memory>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#endif
namespace rr {
namespace {
struct PresentationFramebuffer {
    SDL_Texture* texture = nullptr;
    int width = 0, height = 0;
    ~PresentationFramebuffer() { if (texture) SDL_DestroyTexture(texture); }
};
std::unordered_map<SDL_Renderer*, std::unique_ptr<PresentationFramebuffer>> framebuffers;
}
SDL_RendererLogicalPresentation presentation_mode(SDL_Renderer* renderer) {
    SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(renderer, nullptr, nullptr, &mode);
    return SDL_RendererLogicalPresentation(SDL_GetNumberProperty(SDL_GetRendererProperties(renderer),
                                                                  "rr.presentation.mode", mode));
}
bool presentation_set_logical(SDL_Renderer* renderer, int width, int height,
                              SDL_RendererLogicalPresentation mode) {
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), "rr.presentation.mode", mode);
    // Composition maps logical coordinates onto the selected native target.
    // Integer/fit/stretch apply only when presenting that target to the window.
    const auto internal = framebuffers.count(renderer) ? SDL_LOGICAL_PRESENTATION_STRETCH : mode;
    return SDL_SetRenderLogicalPresentation(renderer, width, height, internal);
}
bool presentation_framebuffer(SDL_Renderer* renderer, int width, int height) {
    if (width <= 0 || height <= 0) return SDL_SetError("Invalid presentation framebuffer size");
    auto& frame = framebuffers[renderer];
    if (!frame) frame = std::make_unique<PresentationFramebuffer>();
    if (!frame->texture || frame->width != width || frame->height != height) {
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                 SDL_TEXTUREACCESS_TARGET, width, height);
        if (!texture) return false;
        int lw = 0, lh = 0;
        SDL_RendererLogicalPresentation mode;
        SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, &mode);
        mode = presentation_mode(renderer);
        SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), "rr.presentation.mode", mode);
        if (!SDL_SetRenderTarget(renderer, texture)) { SDL_DestroyTexture(texture); return false; }
        if (frame->texture) SDL_DestroyTexture(frame->texture);
        frame->texture = texture;
        frame->width = width;
        frame->height = height;
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        if (!SDL_SetRenderLogicalPresentation(renderer, lw, lh, SDL_LOGICAL_PRESENTATION_STRETCH)) return false;
    } else if (SDL_GetRenderTarget(renderer) != frame->texture) {
        if (!SDL_SetRenderTarget(renderer, frame->texture)) return false;
    }
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), "rr.framebuffer.width", width);
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), "rr.framebuffer.height", height);
    return true;
}
SDL_FRect presentation_rectangle(int w, int h, int nw, int nh, SDL_RendererLogicalPresentation mode) {
    float factor = std::min(float(w) / nw, float(h) / nh);
    if (mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE && factor >= 1) factor = std::floor(factor);
    float dw = mode == SDL_LOGICAL_PRESENTATION_STRETCH ? float(w) : nw * factor;
    float dh = mode == SDL_LOGICAL_PRESENTATION_STRETCH ? float(h) : nh * factor;
    return {(w - dw) / 2, (h - dh) / 2, dw, dh};
}
namespace {
SDL_FRect framebuffer_destination(SDL_Renderer* renderer, int w, int h, int fw, int fh,
                                  SDL_RendererLogicalPresentation mode) {
    const bool inset = SDL_GetBooleanProperty(SDL_GetRendererProperties(renderer), "rr.presentation.inset", false);
    auto rect = presentation_rectangle(w, h, fw * (inset ? 2 : 1), fh * (inset ? 2 : 1), mode);
    if (inset) { rect.x += rect.w / 4; rect.y += rect.h / 4; rect.w /= 2; rect.h /= 2; }
    return rect;
}
}
bool presentation_coordinates_from_window(SDL_Renderer* renderer, float x, float y, float* rx, float* ry) {
    const auto props = SDL_GetRendererProperties(renderer);
    const int fw = int(SDL_GetNumberProperty(props, "rr.framebuffer.width", 0));
    const int fh = int(SDL_GetNumberProperty(props, "rr.framebuffer.height", 0));
    if (fw <= 0 || fh <= 0) return SDL_RenderCoordinatesFromWindow(renderer, x, y, rx, ry);
    int lw = 0, lh = 0, w = 0, h = 0, ww = 0, wh = 0;
    SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, &mode);
    if (lw < 2 || lh < 2) return SDL_RenderCoordinatesFromWindow(renderer, x, y, rx, ry);
    SDL_GetWindowSizeInPixels(SDL_GetRenderWindow(renderer), &w, &h);
    SDL_GetWindowSize(SDL_GetRenderWindow(renderer), &ww, &wh);
    auto rect = framebuffer_destination(renderer, w, h, fw, fh, presentation_mode(renderer));
    if (ww <= 0 || wh <= 0 || rect.w <= 0 || rect.h <= 0) return false;
    if (rx) *rx = (x * w / ww - rect.x) * lw / rect.w;
    if (ry) *ry = (y * h / wh - rect.y) * lh / rect.h;
    return true;
}
SDL_Surface* read_presentation_frame(SDL_Renderer* renderer) {
    int w = 0, h = 0;
    SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(renderer, &w, &h, &mode);
    SDL_Rect viewport;
    SDL_GetRenderViewport(renderer, &viewport);
    const bool viewport_set = SDL_RenderViewportSet(renderer);
    float sx = 1, sy = 1;
    SDL_GetRenderScale(renderer, &sx, &sy);
    SDL_FlushRenderer(renderer);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    SDL_SetRenderLogicalPresentation(renderer, w, h, mode);
    SDL_SetRenderScale(renderer, sx, sy);
    SDL_SetRenderViewport(renderer, viewport_set ? &viewport : nullptr);
    return surface;
}
#ifdef _WIN32
namespace {
struct Backend {
    IDirect3D9* api = nullptr;
    IDirect3DDevice9* device = nullptr;
    IDirect3DTexture9* texture = nullptr;
    HWND hwnd = nullptr;
    D3DPRESENT_PARAMETERS parameters{};
    int texture_w = 0, texture_h = 0;
    bool vsync = true;
    ~Backend() {
        if (texture) texture->Release();
        if (device) device->Release();
        if (api) api->Release();
    }
    bool reset(int w, int h) {
        parameters.BackBufferWidth = UINT(w);
        parameters.BackBufferHeight = UINT(h);
        parameters.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
        return SUCCEEDED(device->Reset(&parameters));
    }
    bool initialize(SDL_Window* window) {
        hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!hwnd) return false;
        api = Direct3DCreate9(D3D_SDK_VERSION);
        if (!api) return false;
        RECT r{};
        GetClientRect(hwnd, &r);
        parameters.Windowed = TRUE;
        parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        parameters.hDeviceWindow = hwnd;
        parameters.BackBufferFormat = D3DFMT_UNKNOWN;
        parameters.BackBufferWidth = UINT(r.right);
        parameters.BackBufferHeight = UINT(r.bottom);
        parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        // Preserve simulation floating-point precision; D3D9 otherwise changes the FPU.
        DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE;
        HRESULT hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &parameters, &device);
        if (FAILED(hr))
            hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &parameters,
                                   &device);
        return SUCCEEDED(hr);
    }
    bool present(SDL_Renderer* renderer) {
        RECT client{};
        GetClientRect(hwnd, &client);
        if (client.right <= 0 || client.bottom <= 0) return true;
        HRESULT state = device->TestCooperativeLevel();
        if (state == D3DERR_DEVICELOST) return true;
        if (state == D3DERR_DEVICENOTRESET || int(parameters.BackBufferWidth) != client.right ||
            int(parameters.BackBufferHeight) != client.bottom) {
            if (!reset(client.right, client.bottom)) return true;
        }
        int logical_w = 640, logical_h = 480;
        SDL_RendererLogicalPresentation mode;
        SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &mode);
        mode = presentation_mode(renderer);
        // Read the game viewport, not a desktop-sized image with SDL's
        // presentation already baked in. D3D applies the output scale once.
        SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
        if (!raw) return false;
        SDL_Surface* frame = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!frame) return false;
        float left = -.5f, top = -.5f, right = float(client.right) - .5f, bottom = float(client.bottom) - .5f;
        if (logical_w > 0 && logical_h > 0 && mode != SDL_LOGICAL_PRESENTATION_DISABLED) {
            const auto props = SDL_GetRendererProperties(renderer);
            const int native_w = int(SDL_GetNumberProperty(props, "rr.framebuffer.width", logical_w));
            const int native_h = int(SDL_GetNumberProperty(props, "rr.framebuffer.height", logical_h));
            if (frame->w != native_w || frame->h != native_h) {
                auto* native = SDL_CreateSurface(native_w, native_h, SDL_PIXELFORMAT_ARGB8888);
                if (!native) {
                    SDL_DestroySurface(frame);
                    return false;
                }
                SDL_BlitSurfaceScaled(frame, nullptr, native, nullptr, SDL_SCALEMODE_NEAREST);
                SDL_DestroySurface(frame);
                frame = native;
            }
            auto rect = framebuffer_destination(renderer, client.right, client.bottom, native_w, native_h, mode);
            left = rect.x - .5f;
            top = rect.y - .5f;
            right = left + rect.w;
            bottom = top + rect.h;
        }
        SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), "rr.framebuffer.width", frame->w);
        SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), "rr.framebuffer.height", frame->h);
        if (!texture || texture_w != frame->w || texture_h != frame->h) {
            if (texture) {
                texture->Release();
                texture = nullptr;
            }
            texture_w = frame->w;
            texture_h = frame->h;
            if (FAILED(device->CreateTexture(UINT(texture_w), UINT(texture_h), 1, 0, D3DFMT_A8R8G8B8,
                                             D3DPOOL_MANAGED, &texture, nullptr))) {
                SDL_DestroySurface(frame);
                return false;
            }
        }
        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0))) {
            SDL_DestroySurface(frame);
            return false;
        }
        for (int y = 0; y < frame->h; ++y)
            std::memcpy(static_cast<char*>(locked.pBits) + y * locked.Pitch,
                        static_cast<char*>(frame->pixels) + y * frame->pitch, size_t(frame->w) * 4);
        texture->UnlockRect(0);
        SDL_DestroySurface(frame);
        struct Vertex {
            float x, y, z, rhw, u, v;
        };
        const Vertex vertices[] = {{left, top, 0, 1, 0, 0},
                                   {right, top, 0, 1, 1, 0},
                                   {left, bottom, 0, 1, 0, 1},
                                   {right, bottom, 0, 1, 1, 1}};
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0);
        if (FAILED(device->BeginScene())) return false;
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetTexture(0, texture);
        const auto filter = SDL_GetBooleanProperty(SDL_GetRendererProperties(renderer),
                                                   "rr.presentation.smooth", false) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
        device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        HRESULT hr = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
        device->EndScene();
        if (FAILED(hr)) return false;
        {
            RuntimeTiming timing("presentation.dx9.present");
            hr = device->Present(nullptr, nullptr, nullptr, nullptr);
        }
        return SUCCEEDED(hr) || hr == D3DERR_DEVICELOST;
    }
};
std::unordered_map<SDL_Renderer*, std::unique_ptr<Backend>> backends;
} // namespace
#endif
bool create_game_window_renderer(const char* title, int w, int h, SDL_WindowFlags flags, SDL_Window** window,
                                 SDL_Renderer** renderer) {
#ifdef _WIN32
    if (port_config().directx9 &&
        std::strcmp(SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "", "dummy") != 0) {
        *window = SDL_CreateWindow(title, w, h, flags);
        if (!*window) return false;
        *renderer = SDL_CreateRenderer(*window, "software");
        if (!*renderer) {
            SDL_DestroyWindow(*window);
            *window = nullptr;
            return false;
        }
        controller_set_window(*window);
        auto backend = std::make_unique<Backend>();
        if (!backend->initialize(*window)) {
            SDL_DestroyRenderer(*renderer);
            SDL_DestroyWindow(*window);
            *renderer = nullptr;
            *window = nullptr;
            return SDL_SetError("Direct3D 9 device initialization failed");
        }
        backends[*renderer] = std::move(backend);
        SDL_Log("Presentation: Direct3D 9, main window only");
        return true;
    }
#endif
    const bool ok = SDL_CreateWindowAndRenderer(title, w, h, flags, window, renderer);
    if (ok) controller_set_window(*window);
    return ok;
}
bool present_frame(SDL_Renderer* renderer) {
    RuntimeTiming timing("presentation.total");
#ifdef _WIN32
    auto it = backends.find(renderer);
    if (it != backends.end()) return it->second->present(renderer);
#endif
    const auto found = framebuffers.find(renderer);
    if (found != framebuffers.end()) {
        const auto& frame = *found->second;
        int w = 0, h = 0;
        SDL_RendererLogicalPresentation mode;
        SDL_GetRenderLogicalPresentation(renderer, nullptr, nullptr, &mode);
        mode = presentation_mode(renderer);
        SDL_FlushRenderer(renderer);
        if (!SDL_SetRenderTarget(renderer, nullptr)) return false;
        SDL_GetWindowSizeInPixels(SDL_GetRenderWindow(renderer), &w, &h);
        const auto rect = framebuffer_destination(renderer, w, h, frame.width, frame.height, mode);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderViewport(renderer, nullptr);
        SDL_SetRenderClipRect(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const bool ok = SDL_RenderClear(renderer) &&
                        SDL_RenderTexture(renderer, frame.texture, nullptr, &rect) &&
                        SDL_RenderPresent(renderer);
        const bool restored = SDL_SetRenderTarget(renderer, frame.texture);
        return ok && restored;
    }

    return SDL_RenderPresent(renderer);
}
void destroy_game_renderer(SDL_Renderer* renderer) {
    SDL_SetRenderTarget(renderer, nullptr);
    framebuffers.erase(renderer);
#ifdef _WIN32
    backends.erase(renderer);
#endif
    SDL_DestroyRenderer(renderer);
}
void presentation_vsync(SDL_Renderer* renderer, int vsync) {
#ifdef _WIN32
    auto it = backends.find(renderer);
    if (it != backends.end()) {
        auto& b = *it->second;
        if (b.vsync != (vsync != 0)) {
            b.vsync = vsync != 0;
            b.reset(int(b.parameters.BackBufferWidth), int(b.parameters.BackBufferHeight));
        }
        return;
    }
#endif
    SDL_SetRenderVSync(renderer, vsync);
}
void presentation_low_resolution(SDL_Renderer* renderer, bool low) {
    int lw = 640, lh = 480;
    SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, nullptr);
    const int divisor = low ? 2 : 1;
    presentation_framebuffer(renderer, std::max(1, lw / divisor), std::max(1, lh / divisor));
}
bool uses_directx9(SDL_Renderer* renderer) {
#ifdef _WIN32
    return backends.find(renderer) != backends.end();
#else
    (void)renderer;
    return false;
#endif
}
} // namespace rr
