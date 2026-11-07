#include "settings.hpp"
#include "input.hpp"
#include <algorithm>
#include <cstdlib>

namespace rr {
void MenuStickInput::suspend() {
    direction_ = Direction::none;
    next_repeat_ = 0;
    require_neutral_ = true;
}

MenuDirections MenuStickInput::update(int x, int y, uint64_t now_ms) {
    // SDL axis values are signed 16-bit; use int so abs(-32768) is defined.
    const int ax = std::abs(x), ay = std::abs(y);
    if (ax <= 8000 && ay <= 8000) {
        direction_ = Direction::none;
        require_neutral_ = false;
        return {};
    }
    if (require_neutral_) return {};
    const int threshold = direction_ == Direction::none ? steer_dead_zone : 8000;
    if (ax <= threshold && ay <= threshold) return {};
    const Direction next =
        ax >= ay ? (x < 0 ? Direction::left : Direction::right) : (y < 0 ? Direction::up : Direction::down);
    if (next != direction_) {
        direction_ = next;
        next_repeat_ = now_ms + 350;
    } else {
        if (now_ms < next_repeat_) return {};
        // At most one navigation step per frame, even after a long stall.
        next_repeat_ = now_ms + 100;
    }
    return {next == Direction::left, next == Direction::right, next == Direction::up,
            next == Direction::down};
}

uint32_t controls_from(const PlayerInput& in, int dead_zone) {
    uint32_t c = 0;
    if (in.accelerate) c |= control_accelerate;
    if (in.brake) c |= control_brake;
    if (in.punch) c |= control_punch;
    if (in.kick) c |= control_kick;
    if (in.backhand) c |= control_backhand;
    if (in.nitro) c |= control_nitro;
    if (in.lean_left) c |= control_lean_down;
    if (in.lean_right) c |= control_lean_up;
    if (in.left || in.steer_axis < -dead_zone)
        c |= control_left;
    else if (in.right || in.steer_axis > dead_zone)
        c |= control_right;
    return c;
}

namespace {
// Original action-table order; the unused dismount row has no native bit.
constexpr uint32_t kActionBits[11] = {control_accelerate, control_brake,    control_left,
                                      control_lean_down,  control_nitro,    control_punch,
                                      control_kick,       control_backhand, 0,
                                      control_right,      control_lean_up};
constexpr int kDefaultKeys[11][2] = {{SDL_SCANCODE_UP, SDL_SCANCODE_W},
                                     {SDL_SCANCODE_DOWN, SDL_SCANCODE_S},
                                     {SDL_SCANCODE_LEFT, SDL_SCANCODE_A},
                                     {SDL_SCANCODE_Q, -1},
                                     {SDL_SCANCODE_SPACE, -1},
                                     {SDL_SCANCODE_Z, SDL_SCANCODE_LCTRL},
                                     {SDL_SCANCODE_X, SDL_SCANCODE_LALT},
                                     {SDL_SCANCODE_C, -1},
                                     {-1, -1},
                                     {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D},
                                     {SDL_SCANCODE_E, -1}};
bool pressed(const bool* down, int sc) {
    return sc > 0 && sc < SDL_SCANCODE_COUNT && down[sc];
}
} // namespace

uint32_t bound_control_bits(const std::array<uint16_t, 11>& codes, bool custom, const bool* down) {
    uint32_t bits = 0;
    for (int i = 0; i < 11; ++i) {
        if (kActionBits[i] == 0) continue;
        if (!custom) {
            if (pressed(down, kDefaultKeys[i][0]) || pressed(down, kDefaultKeys[i][1]))
                bits |= kActionBits[i];
        } else {
            // Custom tables store complete SDL scancodes.
            if (pressed(down, codes[size_t(i)] < SDL_SCANCODE_COUNT ? codes[size_t(i)] : kDefaultKeys[i][0]))
                bits |= kActionBits[i];
        }
    }
    return bits;
}

std::array<uint16_t, 11> modern_keyboard_controls() {
    std::array<uint16_t, 11> out{};
    for (int i = 0; i < 11; ++i)
        if (kDefaultKeys[i][0] > 0) out[size_t(i)] = uint16_t(kDefaultKeys[i][0]);
    return out;
}
int controller_capture_binding(const SDL_Event& e) {
    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) return int(e.gbutton.button) + 1;
    if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && std::abs(int(e.gaxis.value)) > 18000)
        return 1001 + int(e.gaxis.axis) * 2 + (e.gaxis.value > 0 ? 1 : 0);
    return 0;
}
int controller_default_binding(int action, bool xinput) {
    constexpr int defaults[] = {1012,
                                1010,
                                1001,
                                1002,
                                1005,
                                1006,
                                SDL_GAMEPAD_BUTTON_LEFT_SHOULDER + 1,
                                SDL_GAMEPAD_BUTTON_WEST + 1,
                                SDL_GAMEPAD_BUTTON_NORTH + 1,
                                SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER + 1};
    if (!xinput) {
        if (action == 0) return SDL_GAMEPAD_BUTTON_SOUTH + 1;
        if (action == 1) return SDL_GAMEPAD_BUTTON_EAST + 1;
        if (action == 4) return SDL_GAMEPAD_BUTTON_DPAD_LEFT + 1;
        if (action == 5) return SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
    }
    return action >= 0 && action < 10 ? defaults[action] : 0;
}
uint32_t controller_control_bits(ControllerDevice* pad, const Settings& settings) {
    if (!pad || !controller_connected(pad)) return 0;
    const bool xinput = controller_xinput(pad);
    uint32_t result = 0;
    for (size_t i = 0; i < binding_actions.size(); ++i) {
        int code = settings.controller_bindings[i];
        if (code < 0) {
            int action = int(i);
            if (settings.swap_punch_kick && (i == 7 || i == 8)) action = i == 7 ? 8 : 7;
            code = controller_default_binding(action, xinput);
        }
        bool pressed = false;
        if (code > 0 && code <= 160)
            pressed = controller_button(pad, code - 1);
        else if (code >= 1001 && code < 1001 + SDL_GAMEPAD_AXIS_COUNT * 2) {
            const int axis = (code - 1001) / 2;
            const int sign = (code - 1001) % 2 ? 1 : -1;
            const int zone =
                axis >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 8000 : settings.stick_deadzone * 32768 / 100;
            pressed = int(controller_axis(pad, SDL_GamepadAxis(axis))) * sign > zone;
        }
        if (pressed) result |= kActionBits[size_t(binding_actions[i])];
    }
    return result;
}
std::string controller_binding_name(int action, int code, bool xinput) {
    if (code < 0) code = controller_default_binding(action, xinput);
    if (!code) return "Unbound";
    if (code >= 1001) {
        constexpr const char* names[] = {"Left stick left",
                                         "Left stick right",
                                         "Left stick up",
                                         "Left stick down",
                                         "Right stick left",
                                         "Right stick right",
                                         "Right stick up",
                                         "Right stick down",
                                         "LT -",
                                         "LT",
                                         "RT -",
                                         "RT"};
        const int index = code - 1001;
        return index >= 0 && index < 12 ? names[index] : "Axis";
    }
    const int button = code - 1;
    if (xinput) {
        constexpr const char* names[] = {"A",     "B",        "X",          "Y",          "Back",
                                         "Guide", "Start",    "L stick",    "R stick",    "LB",
                                         "RB",    "D-pad up", "D-pad down", "D-pad left", "D-pad right"};
        if (button < int(std::size(names))) return names[button];
    }
    if (!xinput) {
        constexpr int map[] = {SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,
                               SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_NORTH,
                               SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
                               SDL_GAMEPAD_BUTTON_BACK,          SDL_GAMEPAD_BUTTON_START,
                               SDL_GAMEPAD_BUTTON_LEFT_STICK,    SDL_GAMEPAD_BUTTON_RIGHT_STICK};
        for (int i = 0; i < 10; ++i)
            if (map[i] == button) return "Button " + std::to_string(i + 1);
        if (button >= 32) return "Button " + std::to_string(button - 32 + 11);
    }
    return "Button " + std::to_string(code);
}

std::string key_label(int scancode, KeyFromScancode lookup) {
    if (scancode <= 0 || scancode >= SDL_SCANCODE_COUNT) return "?";
    const SDL_Keycode key = lookup(SDL_Scancode(scancode), SDL_KMOD_NONE, false);
    const char* name = key != SDLK_UNKNOWN ? SDL_GetKeyName(key) : nullptr;
    if (!name || !*name) name = SDL_GetScancodeName(SDL_Scancode(scancode));
    return name && *name ? name : "?";
}

int default_key_scancode(int action) {
    return action >= 0 && action < 11 ? kDefaultKeys[action][0] : -1;
}

PadLabels pad_labels(SDL_GamepadType type, SDL_GamepadButtonLabel south, SDL_GamepadButtonLabel east,
                     SDL_GamepadButtonLabel west, SDL_GamepadButtonLabel north) {
    PadLabels l;
    const auto face = [](SDL_GamepadButtonLabel label, const char* positional) -> std::string {
        switch (label) {
        case SDL_GAMEPAD_BUTTON_LABEL_A:
            return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B:
            return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X:
            return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y:
            return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
            return "Cross";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
            return "Circle";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
            return "Square";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
            return "Triangle";
        default:
            return positional;
        }
    };
    l.south = face(south, "South");
    l.east = face(east, "East");
    l.west = face(west, "West");
    l.north = face(north, "North");
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
        l.start = "Start", l.back = "Back";
        break;
    case SDL_GAMEPAD_TYPE_XBOXONE:
        l.start = "Menu", l.back = "View";
        break;
    case SDL_GAMEPAD_TYPE_PS3:
        l.start = "Start", l.back = "Select";
        l.left_trigger = "L2", l.right_trigger = "R2", l.right_shoulder = "R1";
        break;
    case SDL_GAMEPAD_TYPE_PS4:
        l.start = "Options", l.back = "Share";
        l.left_trigger = "L2", l.right_trigger = "R2", l.right_shoulder = "R1";
        break;
    case SDL_GAMEPAD_TYPE_PS5:
        l.start = "Options", l.back = "Create";
        l.left_trigger = "L2", l.right_trigger = "R2", l.right_shoulder = "R1";
        break;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        l.start = "+", l.back = "-";
        l.left_trigger = "ZL", l.right_trigger = "ZR", l.right_shoulder = "R";
        break;
    default:
        break;
    }
    return l;
}

PadLabels pad_labels(ControllerDevice* pad) {
#ifdef _WIN32
    if (!pad) return {};
    if (controller_xinput(pad)) return {"A", "B", "X", "Y", "Start", "Back", "LT", "RT", "RB"};
    return {"Button 1", "Button 2", "Button 3", "Button 4", "Button 8",
            "Button 7", "Button 2", "Button 1", "Button 6", false};
#else
    if (!pad) return {};
    return pad_labels(SDL_GetGamepadType(pad), SDL_GetGamepadButtonLabel(pad, SDL_GAMEPAD_BUTTON_SOUTH),
                      SDL_GetGamepadButtonLabel(pad, SDL_GAMEPAD_BUTTON_EAST),
                      SDL_GetGamepadButtonLabel(pad, SDL_GAMEPAD_BUTTON_WEST),
                      SDL_GetGamepadButtonLabel(pad, SDL_GAMEPAD_BUTTON_NORTH));
#endif
}

std::array<std::string, 4> gamepad_rows(const PadLabels& l, bool swap) {
    // race_screen: South/RT gas, East/LT brake, d-pad or stick steer, West
    // punch and North kick (traded by the swap), right shoulder backhand;
    // Start pauses, Back asks to quit a career race; menus take South/Start
    // to select and East/Back to go back.
    return {
        l.south + "/" + l.right_trigger + ": gas  " + l.east + "/" + l.left_trigger + ": brake  Pad: steer",
        (swap ? l.north : l.west) + ": punch  " + (swap ? l.west : l.north) + ": kick  " + l.right_shoulder +
            ": backhand",
        l.start + ": pause  " + l.back + ": quit career race", "Menus: D-pad + " + l.south + "/" + l.east};
}

bool track_gamepad(GamepadPtr& pad, const SDL_Event& e) {
    if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) {
        pad = controller_open(e.gdevice.which);
        return false;
    }
    if (e.type != SDL_EVENT_GAMEPAD_REMOVED || !pad || controller_id(pad.get()) != e.gdevice.which)
        return false;
    pad = controller_first();
    return true;
}

void pad_text_edit(std::string& buffer, int& cursor, PadTextMove move, size_t max_len) {
    // Space first, so a new slot starts at 'A' going up.
    static constexpr char cycle[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-'!?&";
    constexpr int count = int(sizeof cycle) - 1;
    const int last = int(max_len) - 1;
    if (cursor < 0) cursor = std::min(int(buffer.size()), last);
    const int size = int(buffer.size());
    if (move.left) cursor = std::max(0, cursor - 1);
    if (move.right) cursor = std::min({cursor + 1, size, last});
    if (move.up != move.down) {
        const bool fresh = cursor >= size;
        const char current = fresh ? ' ' : buffer[size_t(cursor)];
        const char* found = std::find(cycle, cycle + count, current);
        const int index = found == cycle + count ? 0 : int(found - cycle);
        const char next = cycle[(index + (move.up ? 1 : count - 1)) % count];
        if (fresh)
            buffer.push_back(next);
        else
            buffer[size_t(cursor)] = next;
    }
}

std::string pad_text_caption(const std::string& buffer, int cursor) {
    if (cursor < 0 || size_t(cursor) >= buffer.size()) return buffer + "_";
    const size_t at = size_t(cursor);
    return buffer.substr(0, at) + "[" + buffer[at] + "]" + buffer.substr(at + 1);
}

PlayerInput touch_input(float x, float y, float width, float height, bool left_handed) {
    if (left_handed) x = width - x; // plan 18 Task 7: mirror zones for left thumbs
    PlayerInput in;
    if (y < height / 3) {
        in.punch = true;
        in.accelerate = true;
        return in;
    } // top third: punch
    if (x < width / 3)
        in.left = true;
    else if (x > width * 2 / 3)
        in.right = true;
    else if (y > height * 3 / 4)
        in.brake = true;
    in.accelerate = !in.brake;
    return in;
}

std::array<TouchButton, 8> phone_buttons(bool left_handed) {
    std::array<TouchButton, 8> buttons{{
        {TouchAction::left, 30, 345, 85, 65, "LEFT"},
        {TouchAction::right, 130, 345, 85, 65, "RIGHT"},
        {TouchAction::accelerate, 870, 345, 85, 65, "GAS"},
        {TouchAction::brake, 770, 345, 85, 65, "BRAKE"},
        {TouchAction::punch, 30, 265, 85, 65, "PUNCH"},
        {TouchAction::kick, 130, 265, 85, 65, "KICK"},
        {TouchAction::backhand, 230, 265, 85, 65, "BACKHAND"},
        {TouchAction::pause, 880, 20, 75, 40, "PAUSE"},
    }};
    if (left_handed)
        for (auto& b : buttons) b.x = 1000 - b.x - b.w;
    return buttons;
}

bool touch_contains(const TouchButton& b, float x, float y) {
    return x * 1000 >= b.x && x * 1000 < b.x + b.w && y * 450 >= b.y && y * 450 < b.y + b.h;
}

PlayerInput phone_touch_input(float x, float y, bool left_handed) {
    PlayerInput in;
    for (const auto& b : phone_buttons(left_handed)) {
        if (!touch_contains(b, x, y)) continue;
        switch (b.action) {
        case TouchAction::left:
            in.left = true;
            break;
        case TouchAction::right:
            in.right = true;
            break;
        case TouchAction::accelerate:
            in.accelerate = true;
            break;
        case TouchAction::brake:
            in.brake = true;
            break;
        case TouchAction::punch:
            in.punch = true;
            break;
        case TouchAction::kick:
            in.kick = true;
            break;
        case TouchAction::backhand:
            in.backhand = true;
            break;
        case TouchAction::pause:
            break;
        }
    }
    return in;
}

void TouchInput::handle(const SDL_Event& e) {
    const auto id = std::make_pair(e.tfinger.touchID, e.tfinger.fingerID);
    if (e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED)
        fingers_.erase(id);
    else if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION)
        fingers_[id] = phone_controls_ ? phone_touch_input(e.tfinger.x, e.tfinger.y, left_handed_)
                                       : touch_input(e.tfinger.x, e.tfinger.y, 1.f, 1.f, left_handed_);
}

PlayerInput TouchInput::current() const {
    PlayerInput in;
    for (const auto& finger : fingers_) {
        const auto& f = finger.second;
        in.accelerate |= f.accelerate;
        in.brake |= f.brake;
        in.left |= f.left;
        in.right |= f.right;
        in.punch |= f.punch;
        in.kick |= f.kick;
        in.backhand |= f.backhand;
    }
    // A braking finger overrides auto-throttle from the steering/attack zones.
    if (in.brake) in.accelerate = false;
    return in;
}
} // namespace rr
