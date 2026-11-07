#pragma once
#include "mixer.hpp"
#include "sound_event.hpp"
#include <array>
#include <string>
#include <vector>
namespace rr {
class Race;
struct SoundLevels;
class Audio3DO {
  public:
    Audio3DO(Mixer&, const std::string&);
    ~Audio3DO();
    bool ready() const { return ready_; }
    void start();
    void stop();
    void levels(const SoundLevels&);
    void update(Race&);
    bool event(const SoundEventRecord&);

  private:
    Mixer& mixer_;
    std::array<PcmSound, 19> samples_;
    std::array<int, 5> voices_{{-1, -1, -1, -1, -1}}, resources_{{-1, -1, -1, -1, -1}};
    std::array<int, 2> effect_{{-1, -1}};
    std::array<int, 5> pitches_{};
    std::array<int, 4> steps_{}, gains_{}, pans_{};
    std::vector<int> rpm_, previous_speed_;
    uint32_t seed_ = 1, previous_camera_ = 0;
    bool have_camera_ = false;
    int warmup_ = 0, master_ = 100, engine_ = 80, others_ = 80, effects_ = 80;
    bool ready_ = false, boost_ = false, swap_ = false;
    int random(int);
    void voice(int, int, int, int, int);
};
} // namespace rr
