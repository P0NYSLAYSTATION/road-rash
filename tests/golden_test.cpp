#include "golden.hpp"
#include <iostream>
#include <filesystem>
#include <chrono>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    const std::string ok = "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na.b\t-5\nframe\t1\na.b\t7\nend\n";
    auto f = rr::golden::parse(ok);
    check(f.exe == "abc" && f.scenario == "s" && f.frames.size() == 2, "header and frames");
    check(f.frames[0].at("a.b") == -5 && f.frames[1].at("a.b") == 7, "values");
    for (const char* bad : {"exe\tabc\nframe\t0\nend\n",                              // no magic line
                            "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na\t1\n", // truncated: no end
                            "# rr-oracle 1\nexe\tabc\nscenario\ts\na\t1\nend\n",      // key before frame
                            "# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\na\tx\nend\n"}) { // non-integer
        try {
            rr::golden::parse(bad);
            check(false, bad);
        } catch (const std::runtime_error&) {
        }
    }
    for (const std::string& suffix : {"a\t9223372036854775808\n", "a\t-9223372036854775809\n", "a\t1\na\t2\n",
                                      "frame\t3\n", "exe\tabc\n"}) {
        try {
            rr::golden::parse("# rr-oracle 1\nexe\tabc\nscenario\ts\nframe\t0\n" + suffix + "end\n");
            check(false, "invalid duplicate/range/sequence accepted");
        } catch (const std::runtime_error&) {
        }
    }
    const auto path =
        std::filesystem::temp_directory_path() /
        ("rr-golden-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tsv");
    {
        std::ofstream out(path, std::ios::binary);
        out << "# rr-oracle 1\r\nexe\tabc\r\nscenario\tindexed\r\n";
        for (int i = 0; i < 1000; ++i) out << "frame\t" << i << "\r\na\t" << i * 7 << "\r\n";
        out << "end\r\n";
    }
    {
        const auto lazy = rr::golden::load_indexed(path.string());
        check(lazy.frames.size() == 1000, "indexed frame count");
        const auto& first = lazy.frames.front();
        lazy.frames.pin(37);
        const auto& pinned = lazy.frames.at(37);
        int i = 0;
        for (const auto& frame : lazy.frames) check(frame.at("a") == i++ * 7, "indexed sequential values");
        check(first.at("a") == 0 && pinned.at("a") == 259, "pinned anchors survive full traversal");
        check(lazy.frames.at(11).at("a") == 77 && lazy.frames.back().at("a") == 6993,
              "indexed backward and lookahead access");
        check(lazy.frames.resident_frames() <= 34, "indexed memory bounded independently of race length");
    }
    {
        std::ofstream out(path, std::ios::app);
        out << "frame\t1000\na\t0\n";
    }
    try {
        rr::golden::load_indexed(path.string());
        check(false, "indexed scan must reject invalid late data before replay starts");
    } catch (const std::runtime_error&) {
    }
    std::filesystem::remove(path);
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
