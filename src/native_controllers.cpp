#include "native_controllers.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <xinput.h>
#include <cwchar>
#include <string>
#include <set>
#endif
namespace rr {
ControllerSample xinput_sample(uint16_t b, Sint16 x, Sint16 y, uint8_t left, uint8_t right, Sint16 rx,
                               Sint16 ry) {
    ControllerSample s;
    const std::pair<int, uint16_t> map[] = {{SDL_GAMEPAD_BUTTON_SOUTH, 0x1000},
                                            {SDL_GAMEPAD_BUTTON_EAST, 0x2000},
                                            {SDL_GAMEPAD_BUTTON_WEST, 0x4000},
                                            {SDL_GAMEPAD_BUTTON_NORTH, 0x8000},
                                            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0x0100},
                                            {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0x0200},
                                            {SDL_GAMEPAD_BUTTON_BACK, 0x0020},
                                            {SDL_GAMEPAD_BUTTON_START, 0x0010},
                                            {SDL_GAMEPAD_BUTTON_LEFT_STICK, 0x0040},
                                            {SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0x0080},
                                            {SDL_GAMEPAD_BUTTON_DPAD_UP, 1},
                                            {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 2},
                                            {SDL_GAMEPAD_BUTTON_DPAD_LEFT, 4},
                                            {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 8}};
    for (auto [button, mask] : map) s.buttons[size_t(button)] = (b & mask) != 0;
    s.axes[SDL_GAMEPAD_AXIS_LEFTX] = x;
    s.axes[SDL_GAMEPAD_AXIS_LEFTY] = Sint16(std::clamp(-int(y), -32768, 32767));
    s.axes[SDL_GAMEPAD_AXIS_RIGHTX] = rx;
    s.axes[SDL_GAMEPAD_AXIS_RIGHTY] = Sint16(std::clamp(-int(ry), -32768, 32767));
    s.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = Sint16(int(left) * 32767 / 255);
    s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = Sint16(int(right) * 32767 / 255);
    return s;
}
ControllerSample directinput_sample(int x, int y, uint32_t pov, const uint8_t* buttons) {
    ControllerSample s;
    const int map[] = {SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,
                       SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_NORTH,
                       SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
                       SDL_GAMEPAD_BUTTON_BACK,          SDL_GAMEPAD_BUTTON_START,
                       SDL_GAMEPAD_BUTTON_LEFT_STICK,    SDL_GAMEPAD_BUTTON_RIGHT_STICK};
    for (size_t i = 0; i < std::size(map); ++i) s.buttons[size_t(map[i])] = (buttons[i] & 128) != 0;
    for (int i = 10; i < 128; ++i) s.buttons[size_t(32 + i - 10)] = (buttons[i] & 128) != 0;
    s.axes[SDL_GAMEPAD_AXIS_LEFTX] = Sint16(std::clamp(x, -32768, 32767));
    s.axes[SDL_GAMEPAD_AXIS_LEFTY] = Sint16(std::clamp(y, -32768, 32767));
    if ((pov & 0xffff) != 0xffff && pov < 36000) {
        s.buttons[SDL_GAMEPAD_BUTTON_DPAD_UP] = pov <= 4500 || pov >= 31500;
        s.buttons[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = pov >= 4500 && pov <= 13500;
        s.buttons[SDL_GAMEPAD_BUTTON_DPAD_DOWN] = pov >= 13500 && pov <= 22500;
        s.buttons[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = pov >= 22500 && pov <= 31500;
    }
    // Uncentered throttle/rudder axes are deliberately not interpreted as buttons.
    return s;
}
#ifdef _WIN32
namespace {
struct DirectDevice {
    GUID guid{};
    IDirectInputDevice8W* device = nullptr;
    SDL_JoystickID id = 0;
    ~DirectDevice() {
        if (device) {
            device->Unacquire();
            device->Release();
        }
    }
};
struct State {
    SDL_JoystickID id = 0;
    ControllerSample sample;
    bool xinput = false;
};
struct Driver {
    HWND window = nullptr;
    HMODULE module = nullptr;
    DWORD(WINAPI* get_state)(DWORD, XINPUT_STATE*) = nullptr;
    IDirectInput8W* input = nullptr;
    std::vector<std::unique_ptr<DirectDevice>> devices;
    std::vector<State> states;
    SDL_JoystickID active = 0, next_id = 100;
    ControllerSample previous;
    uint64_t next_scan = 0;
    std::set<DWORD> xinput_products;
    Driver() {
        module = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) module = LoadLibraryExW(L"xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (module)
            get_state = reinterpret_cast<decltype(get_state)>(GetProcAddress(module, "XInputGetState"));
    }
    ~Driver() {
        devices.clear();
        if (input) input->Release();
        if (module) FreeLibrary(module);
    }
    static BOOL CALLBACK axis(const DIDEVICEOBJECTINSTANCEW* object, void* context) {
        auto* device = static_cast<IDirectInputDevice8W*>(context);
        DIPROPRANGE range{};
        range.diph.dwSize = sizeof(range);
        range.diph.dwHeaderSize = sizeof(range.diph);
        range.diph.dwObj = object->dwType;
        range.diph.dwHow = DIPH_BYID;
        range.lMin = -32768;
        range.lMax = 32767;
        device->SetProperty(DIPROP_RANGE, &range.diph);
        return DIENUM_CONTINUE;
    }
    static BOOL CALLBACK enumerate(const DIDEVICEINSTANCEW* info, void* context) {
        auto& self = *static_cast<Driver*>(context);
        if (self.xinput_products.contains(info->guidProduct.Data1)) return DIENUM_CONTINUE;
        for (const auto& d : self.devices)
            if (IsEqualGUID(d->guid, info->guidInstance)) return DIENUM_CONTINUE;
        auto d = std::make_unique<DirectDevice>();
        d->guid = info->guidInstance;
        if (FAILED(self.input->CreateDevice(d->guid, &d->device, nullptr))) return DIENUM_CONTINUE;
        DIPROPGUIDANDPATH path{};
        path.diph.dwSize = sizeof(path);
        path.diph.dwHeaderSize = sizeof(path.diph);
        path.diph.dwHow = DIPH_DEVICE;
        if (SUCCEEDED(d->device->GetProperty(DIPROP_GUIDANDPATH, &path.diph)) && wcsstr(path.wszPath, L"IG_"))
            return DIENUM_CONTINUE;
        if (FAILED(d->device->SetDataFormat(&c_dfDIJoystick2)) ||
            FAILED(d->device->SetCooperativeLevel(self.window, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
            return DIENUM_CONTINUE;
        d->device->EnumObjects(axis, d->device, DIDFT_AXIS);
        DIPROPDWORD dead{};
        dead.diph.dwSize = sizeof(dead);
        dead.diph.dwHeaderSize = sizeof(dead.diph);
        dead.diph.dwHow = DIPH_DEVICE;
        dead.dwData = 0;
        d->device->SetProperty(DIPROP_DEADZONE, &dead.diph);
        d->id = self.next_id++;
        self.devices.push_back(std::move(d));
        return DIENUM_CONTINUE;
    }
    void scan() {
        if (!input &&
            FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                      reinterpret_cast<void**>(&input), nullptr)))
            return;
        xinput_products.clear();
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == 0) {
            std::vector<RAWINPUTDEVICELIST> raw(count);
            if (GetRawInputDeviceList(raw.data(), &count, sizeof(RAWINPUTDEVICELIST)) != UINT(-1))
                for (const auto& d : raw) {
                    UINT size = 0;
                    GetRawInputDeviceInfoW(d.hDevice, RIDI_DEVICENAME, nullptr, &size);
                    if (!size || size > 4096) continue;
                    std::wstring path(size, L'\0');
                    if (GetRawInputDeviceInfoW(d.hDevice, RIDI_DEVICENAME, path.data(), &size) == UINT(-1) ||
                        path.find(L"IG_") == std::wstring::npos)
                        continue;
                    auto v = path.find(L"VID_"), p = path.find(L"PID_");
                    unsigned vid = 0, pid = 0;
                    if (v != path.npos && p != path.npos &&
                        swscanf_s(path.c_str() + v + 4, L"%4x", &vid) == 1 &&
                        swscanf_s(path.c_str() + p + 4, L"%4x", &pid) == 1)
                        xinput_products.insert(MAKELONG(vid, pid));
                }
        }
        devices.erase(std::remove_if(devices.begin(), devices.end(),
                                     [&](const auto& d) { return FAILED(input->GetDeviceStatus(d->guid)); }),
                      devices.end());
        input->EnumDevices(DI8DEVCLASS_GAMECTRL, enumerate, this, DIEDFL_ATTACHEDONLY);
    }
    const State* find(SDL_JoystickID id) const {
        for (const auto& s : states)
            if (s.id == id) return &s;
        return nullptr;
    }
    void poll() {
        if (!window) return;
        const bool focused = GetForegroundWindow() == window;
        const auto now = SDL_GetTicks();
        if (now >= next_scan) {
            scan();
            next_scan = now + 2000;
        }
        states.clear();
        if (get_state)
            for (DWORD i = 0; i < 4; ++i) {
                XINPUT_STATE state{};
                if (get_state(i, &state) != ERROR_SUCCESS) continue;
                State s;
                s.id = i + 1;
                s.xinput = true;
                if (focused)
                    s.sample =
                        xinput_sample(state.Gamepad.wButtons, state.Gamepad.sThumbLX, state.Gamepad.sThumbLY,
                                      state.Gamepad.bLeftTrigger, state.Gamepad.bRightTrigger,
                                      state.Gamepad.sThumbRX, state.Gamepad.sThumbRY);
                states.push_back(s);
            }
        for (auto& d : devices) {
            if (FAILED(input->GetDeviceStatus(d->guid))) continue;
            State state;
            state.id = d->id;
            if (focused) {
                DIJOYSTATE2 js{};
                HRESULT hr = d->device->Poll();
                if (FAILED(hr)) {
                    d->device->Acquire();
                    hr = d->device->Poll();
                }
                if (SUCCEEDED(hr) && SUCCEEDED(d->device->GetDeviceState(sizeof(js), &js))) {
                    state.sample = directinput_sample(js.lX, js.lY, js.rgdwPOV[0], js.rgbButtons);
                    state.sample.axes[SDL_GAMEPAD_AXIS_RIGHTX] = Sint16(js.lRz);
                    state.sample.axes[SDL_GAMEPAD_AXIS_RIGHTY] = Sint16(js.lRx);
                }
            } else
                d->device->Unacquire();
            states.push_back(state);
        }
        SDL_JoystickID chosen = find(active) ? active : states.empty() ? 0 : states.front().id;
        for (const auto& s : states) {
            bool used =
                std::any_of(s.sample.buttons.begin(), s.sample.buttons.end(), [](bool b) { return b; });
            for (auto v : s.sample.axes) used |= std::abs(int(v)) > 8000;
            if (used) {
                chosen = s.id;
                if (s.id == active) break;
            }
        }
        auto device_event = [](SDL_EventType type, SDL_JoystickID id) {
            SDL_Event e{};
            e.type = type;
            e.gdevice.which = id;
            SDL_PushEvent(&e);
        };
        if (chosen != active) {
            if (active) device_event(SDL_EVENT_GAMEPAD_REMOVED, active);
            active = chosen;
            previous = {};
            if (active) device_event(SDL_EVENT_GAMEPAD_ADDED, active);
        }
        const State* s = find(active);
        const ControllerSample current = s ? s->sample : ControllerSample{};
        for (size_t i = 0; i < current.buttons.size(); ++i)
            if (current.buttons[i] != previous.buttons[i]) {
                SDL_Event e{};
                e.type = current.buttons[i] ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
                e.gbutton.which = active;
                e.gbutton.button = Uint8(i);
                e.gbutton.down = current.buttons[i];
                SDL_PushEvent(&e);
            }
        for (size_t i = 0; i < current.axes.size(); ++i)
            if (current.axes[i] != previous.axes[i]) {
                SDL_Event e{};
                e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
                e.gaxis.which = active;
                e.gaxis.axis = Uint8(i);
                e.gaxis.value = current.axes[i];
                SDL_PushEvent(&e);
            }
        previous = current;
    }
};
Driver& driver() {
    static Driver d;
    return d;
}
} // namespace
void controller_set_window(SDL_Window* window) {
    auto& d = driver();
    d.devices.clear();
    d.window = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    d.next_scan = 0;
}
void controller_poll() {
    driver().poll();
}
GamepadPtr controller_open(SDL_JoystickID id) {
    return driver().find(id) ? GamepadPtr(new NativeController{id}) : GamepadPtr{};
}
GamepadPtr controller_first() {
    controller_poll();
    return controller_open(driver().active);
}
bool controller_connected(ControllerDevice* p) {
    return p && p->id == driver().active && driver().find(p->id);
}
SDL_JoystickID controller_id(ControllerDevice* p) {
    return p ? p->id : 0;
}
bool controller_button(ControllerDevice* p, int b) {
    const auto* s = p ? driver().find(p->id) : nullptr;
    return s && driver().active == p->id && b >= 0 && b < 160 && s->sample.buttons[size_t(b)];
}
Sint16 controller_axis(ControllerDevice* p, SDL_GamepadAxis a) {
    const auto* s = p ? driver().find(p->id) : nullptr;
    return s && driver().active == p->id && a >= 0 && a < SDL_GAMEPAD_AXIS_COUNT ? s->sample.axes[size_t(a)]
                                                                                 : 0;
}
bool controller_xinput(ControllerDevice* p) {
    const auto* s = p ? driver().find(p->id) : nullptr;
    return s && s->xinput;
}
#else
void controller_set_window(SDL_Window*) {}
void controller_poll() {}
GamepadPtr controller_open(SDL_JoystickID id) {
    return GamepadPtr(SDL_OpenGamepad(id));
}
GamepadPtr controller_first() {
    int n = 0;
    auto* ids = SDL_GetGamepads(&n);
    GamepadPtr p;
    for (int i = 0; ids && i < n && !p; ++i) p = controller_open(ids[i]);
    SDL_free(ids);
    return p;
}
bool controller_connected(ControllerDevice* p) {
    return SDL_GamepadConnected(p);
}
SDL_JoystickID controller_id(ControllerDevice* p) {
    return SDL_GetGamepadID(p);
}
bool controller_button(ControllerDevice* p, int b) {
    return b < SDL_GAMEPAD_BUTTON_COUNT ? SDL_GetGamepadButton(p, SDL_GamepadButton(b)) : false;
}
Sint16 controller_axis(ControllerDevice* p, SDL_GamepadAxis a) {
    return SDL_GetGamepadAxis(p, a);
}
bool controller_xinput(ControllerDevice*) {
    return true;
}
#endif
} // namespace rr
