#pragma once
// Reader for oracle golden files (docs/plans/01-foundation-and-oracle.md, Task 3).
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <iterator>
#include <algorithm>
#include <utility>

namespace rr::golden {
constexpr int skip = 77;
inline bool original_executable(const std::string& hash) {
    // The second executable changes only its startup compatibility check.
    // Capture receipts verify that patch; simulation/render routines retain
    // the original addresses and instructions.
    return hash == "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" ||
           hash == "b845fc4053f2387c863d3fd71d4b1457cefc2eab900d9562511d8d403b6f7563";
}
inline std::string raster_fixture() {
    if (const char* path = std::getenv("RR_TEST_RASTER_FIXTURE"); path && *path) return path;
    const char* root = std::getenv("RR_ORACLE_DIR");
    return std::string(root && *root ? root : RR_SOURCE_DIR "/analysis/oracle") + "/raster-fixtures.tsv";
}

using Frame = std::map<std::string, int64_t>;

// Full races can contain millions of fields. Indexed replay validates the
// entire file first, then retains only a small working set of decoded frames.
class Frames {
    struct Index {
        std::ifstream input;
        std::vector<std::streamoff> offsets;
        std::map<size_t, Frame> cache;
        std::set<size_t> pinned{0};
        Index(const std::string& path, std::vector<std::streamoff> positions)
            : input(path, std::ios::binary), offsets(std::move(positions)) {
            if (!input) throw std::runtime_error("Cannot reopen golden file " + path);
        }
    };
    std::vector<Frame> stored_;
    std::shared_ptr<Index> index_;

  public:
    void index(const std::string& path, std::vector<std::streamoff> offsets) {
        index_ = std::make_shared<Index>(path, std::move(offsets));
    }
    size_t size() const { return index_ ? index_->offsets.size() : stored_.size(); }
    bool empty() const { return size() == 0; }
    void emplace_back() { stored_.emplace_back(); }
    void pin(size_t i) const {
        if (index_) index_->pinned.insert(i);
    }
    size_t resident_frames() const { return index_ ? index_->cache.size() : stored_.size(); }
    const Frame& at(size_t i) const {
        if (!index_) return stored_.at(i);
        auto& idx = *index_;
        if (auto found = idx.cache.find(i); found != idx.cache.end()) return found->second;
        idx.input.clear();
        idx.input.seekg(idx.offsets.at(i));
        Frame frame;
        std::string line;
        while (std::getline(idx.input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            if (line == "end" || line.rfind("frame\t", 0) == 0) break;
            const auto tab = line.find('\t');
            if (tab == std::string::npos) throw std::runtime_error("Golden file changed during replay");
            const auto value = line.substr(tab + 1);
            char* end = nullptr;
            errno = 0;
            const auto n = std::strtoll(value.c_str(), &end, 10);
            if (value.empty() || *end || errno == ERANGE || !frame.emplace(line.substr(0, tab), n).second)
                throw std::runtime_error("Golden file changed during replay");
        }
        auto result = idx.cache.emplace(i, std::move(frame)).first;
        while (idx.cache.size() > 32 + idx.pinned.size()) {
            auto victim = idx.cache.end();
            size_t farthest = 0;
            for (auto it = idx.cache.begin(); it != idx.cache.end(); ++it) {
                if (it->first == i || idx.pinned.count(it->first)) continue;
                const size_t distance = it->first < i ? i - it->first : it->first - i;
                if (victim == idx.cache.end() || distance > farthest) {
                    victim = it;
                    farthest = distance;
                }
            }
            if (victim == idx.cache.end()) break;
            idx.cache.erase(victim);
        }
        return result->second;
    }
    const Frame& operator[](size_t i) const { return at(i); }
    const Frame& front() const { return at(0); }
    const Frame& back() const { return at(size() - 1); }
    Frame& back() { return const_cast<Frame&>(std::as_const(*this).back()); }
    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Frame;
        using difference_type = std::ptrdiff_t;
        using pointer = const Frame*;
        using reference = const Frame&;
        const Frames* owner;
        size_t i;
        reference operator*() const { return owner->at(i); }
        pointer operator->() const { return &owner->at(i); }
        Iterator& operator++() {
            ++i;
            return *this;
        }
        Iterator operator++(int) {
            auto previous = *this;
            ++i;
            return previous;
        }
        bool operator==(const Iterator& other) const { return owner == other.owner && i == other.i; }
        bool operator!=(const Iterator& other) const { return !(*this == other); }
    };
    Iterator begin() const { return {this, 0}; }
    Iterator end() const { return {this, size()}; }
};
struct File {
    std::string exe, scenario;
    Frames frames;
};

inline File parse_stream(std::istream& in, const std::string& indexed_path = {}) {
    // Accepted schema versions, enumerated explicitly (replay-harness
    // correction Oct 12): v1 stays parseable so old captures never fail
    // on a magic bump. A new version is added here only with its parser.
    static constexpr const char* kVersions[] = {"# rr-oracle 1"};
    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("Golden file: bad magic");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    bool known = false;
    for (const char* v : kVersions)
        if (line == v) known = true;
    if (!known) throw std::runtime_error("Golden file: bad magic");
    File f;
    std::vector<std::streamoff> offsets;
    Frame current;
    const bool indexed = !indexed_path.empty();
    auto frame_count = [&] { return indexed ? offsets.size() : f.frames.size(); };
    bool ended = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (ended) throw std::runtime_error("Golden file: data after end");
        if (line == "end") {
            ended = true;
            continue;
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) throw std::runtime_error("Golden file: missing tab: " + line);
        const std::string key = line.substr(0, tab), value = line.substr(tab + 1);
        if (key == "exe") {
            if (!f.exe.empty() || value.empty() || frame_count())
                throw std::runtime_error("Golden file: invalid exe header");
            f.exe = value;
            continue;
        }
        if (key == "scenario") {
            if (!f.scenario.empty() || value.empty() || frame_count())
                throw std::runtime_error("Golden file: invalid scenario header");
            f.scenario = value;
            continue;
        }
        if (key == "frame") {
            if (f.exe.empty() || f.scenario.empty() || value != std::to_string(frame_count()))
                throw std::runtime_error("Golden file: invalid frame sequence/header");
            if (indexed) {
                offsets.push_back(in.tellg());
                current.clear();
            } else
                f.frames.emplace_back();
            continue;
        }
        if (!frame_count()) throw std::runtime_error("Golden file: key before frame: " + key);
        char* end = nullptr;
        errno = 0;
        const long long v = std::strtoll(value.c_str(), &end, 10);
        if (value.empty() || *end || errno == ERANGE)
            throw std::runtime_error("Golden file: not an integer: " + line);
        if (key.empty() || !(indexed ? current : f.frames.back()).emplace(key, v).second)
            throw std::runtime_error("Golden file: empty or duplicate field");
    }
    if (!ended) throw std::runtime_error("Golden file: truncated (no end marker)");
    if (indexed) f.frames.index(indexed_path, std::move(offsets));
    return f;
}

inline File parse(const std::string& text) {
    std::istringstream in(text);
    return parse_stream(in);
}

inline File load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open golden file " + path);
    return parse_stream(in);
}

inline File load_indexed(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open golden file " + path);
    return parse_stream(in, path);
}

inline std::string oracle_dir() {
    if (const char* d = std::getenv("RR_ORACLE_DIR")) return d;
    return std::string(RR_SOURCE_DIR) + "/analysis/oracle";
}
} // namespace rr::golden
