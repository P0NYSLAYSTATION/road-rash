#pragma once
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace rr {
// Optional wall-clock tracing for stalls that also occur outside simulation.
class RuntimeTiming {
    struct Output {
        FILE* file = nullptr;
        std::mutex mutex;
        Output() {
            if (const char* path = std::getenv("RR_RUNTIME_TIMINGS")) {
                file = std::fopen(path, "wb");
                if (file) std::fputs("stage,start_ns,duration_ns\n", file);
            }
        }
        ~Output() {
            if (file) std::fclose(file);
        }
    };
    static Output& output() {
        static Output value;
        return value;
    }
    const char* stage_;
    Uint64 start_;

  public:
    explicit RuntimeTiming(const char* stage) : stage_(stage), start_(output().file ? SDL_GetTicksNS() : 0) {}
    ~RuntimeTiming() {
        if (!start_) return;
        auto elapsed = SDL_GetTicksNS() - start_;
        auto& out = output();
        std::lock_guard lock(out.mutex);
        std::fprintf(out.file, "%s,%llu,%llu\n", stage_, (unsigned long long)start_,
                     (unsigned long long)elapsed);
    }
    RuntimeTiming(const RuntimeTiming&) = delete;
    RuntimeTiming& operator=(const RuntimeTiming&) = delete;
};
} // namespace rr
