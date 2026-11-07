#include "audio_3do.hpp"
#include "audio_3do_math.hpp"
#include "sound.hpp"
#include "race.hpp"
#include "traffic.hpp"
#include "assets.hpp"
#include "utf8_path.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>
namespace rr {
namespace {
struct Spatial {
    int pitch = 256, gain = 0, pan = 64;
};
Spatial spatial(int pos, int lateral, int speed, int camera, int camera_x, int camera_speed,
                int heading = 0) {
    Spatial s;
    int dp = pos - camera, dl = lateral - camera_x, ap = std::abs(dp), al = std::abs(dl);
    const int distance = ap + al - std::min(ap, al) / 2;
    if (distance >= 0x1e00) return s;
    const int v = (0x1e00 - distance) * 8 / 256;
    s.gain = v * v / 256;
    const int relative = dp < 0 ? speed - camera_speed : camera_speed - speed;
    const int denominator = 0x13b00 - relative;
    s.pitch = denominator > 0 ? int(0x13b0000LL / denominator) : 256;
    int a = (dp < 0x300 ? angle_3do(dp, dl) : heading) / 65536;
    if (a > 192)
        a -= 256;
    else if (a > 64)
        a = 128 - a;
    s.pan = std::clamp(64 + a * 13 / 16, 0, 127);
    return s;
}
} // namespace
Audio3DO::Audio3DO(Mixer& m, const std::string& dir) : mixer_(m) {
    try {
        Assets a(dir);
        for (int i = 0; i < 19; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "resource%02d.wav", i + 1);
            samples_[size_t(i)] = read_wav(a.read(name));
        }
        ready_ = true;
        SDL_Log("3DO audio: native 16-bit sample bank loaded from %s", dir.c_str());
    } catch (const std::exception& e) {
        SDL_Log("3DO audio unavailable; retaining PC audio: %s", e.what());
    }
}
Audio3DO::~Audio3DO() {
    stop();
    for (auto& s : samples_) mixer_.retire(&s);
}
int Audio3DO::random(int limit) {
    seed_ = seed_ * 1103515245u + 12345u;
    return int((seed_ >> 16) & 32767u) % limit;
}
void Audio3DO::levels(const SoundLevels& l) {
    master_ = l.mute ? 0 : std::clamp(l.master, 0, 100);
    engine_ = l.engine;
    others_ = l.others;
    effects_ = l.effects;
    swap_ = l.swap_speakers;
}
void Audio3DO::stop() {
    for (auto& v : voices_) {
        if (v >= 0) mixer_.stop(v);
        v = -1;
    }
    for (auto& v : effect_) {
        if (v >= 0) mixer_.stop(v);
        v = -1;
    }
    resources_.fill(-1);
}
void Audio3DO::start() {
    stop();
    boost_ = false;
    rpm_.clear();
    previous_speed_.clear();
    warmup_ = 180;
    have_camera_ = false;
    pitches_[0] = 0x2000;
    for (int i = 0; i < 4; ++i) {
        pitches_[size_t(i + 1)] = random(0xe000) + 0x2000;
        steps_[size_t(i)] = random(0x1b1) + 0x50;
        gains_[size_t(i)] = i == 0 ? 0x3aaa : i == 1 ? 0x4aaa : i == 2 ? 0x3faa : 0x2faa;
        pans_[size_t(i)] = i == 0 ? 0x2c : i == 1 ? 0x36 : i == 2 ? 0x4a : 0x4f;
    }
}
void Audio3DO::voice(int slot, int resource, int pitch, int gain, int pan) {
    pan = swap_ ? 127 - pan : pan;
    gain = std::clamp(gain, 0, 32767);
    pitch = std::clamp(pitch, 0, 65535);
    int& v = voices_[size_t(slot)];
    if (!gain) {
        if (v >= 0) mixer_.stop(v);
        v = -1;
        return;
    }
    if (resources_[size_t(slot)] != resource) {
        if (v >= 0) mixer_.stop(v);
        v = -1;
        resources_[size_t(slot)] = resource;
    }
    if (v >= 0 && mixer_.active(v))
        mixer_.set_3do(v, gain, pan, pitch);
    else
        v = mixer_.play_3do(&samples_[size_t(resource)], gain, pan, pitch, true);
}
void Audio3DO::update(const Race& race) {
    auto& riders = race.riders();
    if (riders.empty()) return;
    if (rpm_.size() != riders.size()) {
        rpm_.assign(riders.size(), 0x2000);
        previous_speed_.assign(riders.size(), 0);
    }
    if (warmup_ > 0) {
        pitches_[0] = std::clamp(pitches_[0] + ((race.player_controls() & 0x04000000) ? 0x250 : -0x250),
                                 0x2000, 0xffff);
        rpm_[0] = pitches_[0];
        voice(0, 0, pitches_[0], 0x5aaa * engine_ * master_ / 10000, 64);
        for (int i = 0; i < 4; ++i) {
            int& p = pitches_[size_t(i + 1)];
            int& step = steps_[size_t(i)];
            p += step;
            if (p > 0xffff) {
                p = 0xffff;
                step = -(random(0x1b1) + 0x50);
            } else if (step < 0 && p < 0xc000) {
                step = random(0x1b1) + 0x50;
                gains_[size_t(i)] = random(0x2001) + 0x2aaa;
                pans_[size_t(i)] = random(0x41) + 0x20;
            }
            voice(i + 1, 0, p, gains_[size_t(i)] * others_ * master_ / 10000, pans_[size_t(i)]);
        }
        --warmup_;
        if (!warmup_) {
            for (int i = 3; i < 5; ++i) {
                mixer_.stop(voices_[size_t(i)]);
                voices_[size_t(i)] = -1;
            }
        }
        return;
    }
    const int camera = int(race.camera_position()), cx = race.camera_x();
    // The listener is camera motion, not the rider velocity during camera easing.
    const int cs = have_camera_ ? int(int64_t(int32_t(uint32_t(camera) - previous_camera_)) * 256)
                                : riders[0].bike.i32(0x108);
    previous_camera_ = uint32_t(camera);
    have_camera_ = true;
    const int resource = boost_ ? 17 : race.player_in_tunnel() ? 18 : 0;
    auto bike = [&](int slot, size_t index, int level) {
        const auto& b = riders[index].bike;
        // Audio modulation must not change networked simulation/dashboard state.
        auto modulated = b;
        modulated.i32(0x2dc) = rpm_[index];
        modulated.i32(0x2e8) = previous_speed_[index];
        update_engine_rpm(modulated);
        rpm_[index] = modulated.i32(0x2dc);
        previous_speed_[index] = modulated.i32(0x2e8);
        auto s = spatial(b.i32(0x1c), b.i32(0x28) / 256, b.i32(0x108), camera, cx, cs, b.i32(0x128));
        const int amplitude = ((s.gain * 32767 / 512) * 9 / 8) * level * master_ / 10000;
        voice(slot, resource, int(int64_t(s.pitch) * rpm_[index] / 256), amplitude, s.pan);
    };
    bike(0, 0, engine_);
    struct Candidate {
        int distance;
        bool car;
        size_t index;
    };
    std::vector<Candidate> near;
    for (size_t i = 1; i < riders.size(); ++i) {
        int d = std::abs(riders[i].bike.i32(0x1c) - camera);
        if (d < 0x1e00) near.push_back({d, false, i});
    }
    if (auto* traffic = race.traffic())
        for (size_t i = 0; i < traffic->cars().size(); ++i) {
            const auto& c = traffic->cars()[i];
            int d = std::abs(c.position - camera);
            if (c.active && d < 0x1e00) near.push_back({d, true, i});
        }
    // total-order: stable insertion order breaks equal distances (riders, then traffic).
    std::stable_sort(near.begin(), near.end(),
                     [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
    for (int rank = 0; rank < 2; ++rank) {
        if (size_t(rank) >= near.size()) {
            voice(rank + 1, 0, 0, 0, 64);
            continue;
        }
        const auto c = near[size_t(rank)];
        if (!c.car) {
            bike(rank + 1, c.index, others_);
            continue;
        }
        const auto& car = race.traffic()->cars()[c.index];
        const int speed = car.motion.i32(0x108) ? car.motion.i32(0x108) : car.speed;
        auto s = spatial(car.position, car.lateral / 256, speed, camera, cx, cs);
        voice(rank + 1, 1, int(int64_t(s.pitch) * 0xd6d8 / 256),
              (s.gain * 0x3fff / 256) * others_ * master_ / 10000, s.pan);
    }
}
bool Audio3DO::event(const SoundEventRecord& e) {
    if (e.code == SoundEvent::nitro_on) {
        boost_ = true;
        return true;
    }
    if (e.code == SoundEvent::nitro_off) {
        boost_ = false;
        rpm_.clear();
        previous_speed_.clear();
        return true;
    }
    const int code = int(e.code);
    int resource = -1, channel = 0, pitch = 0x62b4 + random(0x1f41), gain = 0x5fff + random(0x2001);
    if (code <= 11)
        resource = code + 2;
    else if (e.code == SoundEvent::footstep) {
        resource = 15;
        pitch = 0x666a + random(0x332d);
        gain = 0x4bc8 + random(0x4b1);
    } else if (e.code == SoundEvent::skid_on) {
        resource = 16;
        gain = 32767;
    } else
        return false; // procedural DSP-only effects retain the PC equivalent.
    if (resource < 0 || resource >= 19) return true;
    if (code <= 2) {
        if (mixer_.active(effect_[0]) && mixer_.active(effect_[1])) return true;
        gain = 0x3fff + random(0x4001);
    } else if (code <= 5) {
        channel = mixer_.active(effect_[1]) ? 0 : 1;
        if (mixer_.active(effect_[channel])) return true;
        pitch = 0x62b4 + random(0x3a99);
    }
    if (effect_[size_t(channel)] >= 0) mixer_.stop(effect_[size_t(channel)]);
    gain = gain * effects_ * master_ / 10000;
    effect_[size_t(channel)] = mixer_.play_3do(&samples_[size_t(resource)], gain, 64, pitch, false);
    return true;
}
} // namespace rr
