#pragma once
#include <memory>
#include <string>
#include <vector>
namespace rr {
std::vector<std::string> soundfont_candidates(const std::string& installation);
class MidiSynth {
  public:
    explicit MidiSynth(const std::string& installation);
    ~MidiSynth();
    void event(unsigned char status, unsigned char a, unsigned char b);
    void render(float* left, float* right, int frames);
    void reset();
    const std::string& soundfont() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rr
