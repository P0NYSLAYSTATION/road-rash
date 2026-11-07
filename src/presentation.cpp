#include "presentation.hpp"
#include "port_config.hpp"
#include <memory>
#include <unordered_map>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#endif
namespace rr {
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
        SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
        if (!raw) return false;
        SDL_Surface* frame = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!frame) return false;
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
        const float w = float(client.right) - .5f, h = float(client.bottom) - .5f;
        const Vertex vertices[] = {
            {-.5f, -.5f, 0, 1, 0, 0}, {w, -.5f, 0, 1, 1, 0}, {-.5f, h, 0, 1, 0, 1}, {w, h, 0, 1, 1, 1}};
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0);
        if (FAILED(device->BeginScene())) return false;
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetTexture(0, texture);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        HRESULT hr = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
        device->EndScene();
        if (FAILED(hr)) return false;
        hr = device->Present(nullptr, nullptr, nullptr, nullptr);
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
    return SDL_CreateWindowAndRenderer(title, w, h, flags, window, renderer);
}
bool present_frame(SDL_Renderer* renderer) {
#ifdef _WIN32
    auto it = backends.find(renderer);
    if (it != backends.end()) return it->second->present(renderer);
#endif
    return SDL_RenderPresent(renderer);
}
void destroy_game_renderer(SDL_Renderer* renderer) {
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
bool uses_directx9(SDL_Renderer* renderer) {
#ifdef _WIN32
    return backends.find(renderer) != backends.end();
#else
    (void)renderer;
    return false;
#endif
}
} // namespace rr
