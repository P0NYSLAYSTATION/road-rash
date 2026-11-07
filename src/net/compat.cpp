// Plan 34 Task 7: the production compatibility gate (see compat.hpp).
#include "net/compat.hpp"
#include "race_setup.hpp"

#include <cctype>
#include <algorithm>
#include <stdexcept>
#include "assets.hpp"
#include "importer.hpp"
#include "sha256.hpp"

namespace rr::net {

uint64_t fnv1a64(const void* data, size_t size) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t sim_source_hash() {
#if defined(RR_SIM_SOURCE_HASH)
    // 64 hex chars of the build-time SHA-256 over the simulation sources;
    // the first 16 are the high bytes, as in tools/net_bots.cpp.
    const char* hex = RR_SIM_SOURCE_HASH;
    uint64_t v = 0;
    for (int i = 0; i < 16 && hex[i]; ++i) {
        char c = hex[i];
        const int d = c >= '0' && c <= '9'   ? c - '0'
                      : c >= 'a' && c <= 'f' ? c - 'a' + 10
                      : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                             : -1;
        if (d < 0) return 0x9e3779b97f4a7c15ull;
        v = (v << 4) | uint64_t(d);
    }
    return v ? v : 0x9e3779b97f4a7c15ull;
#else
    return 0x9e3779b97f4a7c15ull; // development fallback (unit tests)
#endif
}

uint64_t self_test_hash() {
    // Fixed vectors over the deterministic core. Any change to the rider
    // table, the countdown, the protocol version or the codecs changes this.
    std::string buf;
    auto u64 = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            buf.push_back(char(v & 0xff));
            v >>= 8;
        }
    };
    u64(network_self_test_hash());
    u64(protocol_version);
    u64(uint64_t(max_seats));
    u64(0x168); // networked countdown (0x00442dd0), kept from the original
    // The grid/AI table (0x00405600): full 15, limited 8, humans-only humans.
    for (int humans = 1; humans <= 8; ++humans)
        for (int ai = 0; ai <= 2; ++ai) u64(uint64_t(net_rider_count(humans, ai)));
    // Codec round-trips with fixed payloads (rejects silently change wire bytes).
    // NOTE: the Hello here is built by hand, not via make_hello: make_hello
    // fills local_fingerprint(), which calls this function.
    Hello h0;
    h0.protocol = protocol_version;
    h0.sim_fingerprint = 0x0123456789abcdefull;
    h0.platform = Platform::macos;
    h0.alias = "Abc";
    auto bytes = encode(Message(h0));
    const auto back = decode(bytes);
    u64(back && std::get_if<Hello>(&*back) ? 0x12345678ull : 0);
    u64(fnv1a64(bytes.data(), bytes.size()));
    const RoomCreate rc{{2, 2, true, true}};
    auto rb = encode(Message(rc));
    u64(fnv1a64(rb.data(), rb.size()));
    // Room-code codec: bare codes and invite links normalise identically.
    for (const char* t : {"234567", "234567", "https://road-rash.archit.xyz/?room=234567"}) {
        const auto code = parse_room_code(t);
        u64(code ? fnv1a64(code->data(), code->size()) : 0);
    }
    if (!valid_room_code("234567") || valid_room_code("000000") || valid_room_code("AAAAA") ||
        parse_room_code("https://road-rash.archit.xyz/?room=OOOOOO").has_value())
        u64(0xdeadbeefull); // alphabet violations must stay rejected
    return fnv1a64(buf.data(), buf.size());
}

DataDigest digest_from_table(const std::vector<std::pair<std::string, std::string>>& rows) {
    auto sorted = rows;
    std::sort(sorted.begin(), sorted.end()); // total-order: string pairs
    std::string buf;
    for (const auto& [path, sha] : sorted) {
        buf += path + '\n';
        for (char c : sha) buf.push_back(char(std::tolower(uint8_t(c))));
        buf += '\n';
    }
    auto hex = sha256(std::vector<uint8_t>(buf.begin(), buf.end()));
    DataDigest digest{};
    for (size_t i = 0; i < digest.size(); ++i)
        digest[i] = uint8_t(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    return digest;
}
DataDigest gameplay_data_digest(const Assets& assets, const VerificationProgress& progress) {
    std::vector<std::pair<std::string, std::string>> rows;
    std::vector<std::string> paths;
    for (const auto& reference : reference_hashes())
        if (reference.path == "ROADRASH.EXE" || reference.path.starts_with("DATA/"))
            paths.push_back(reference.path);
    if (progress) progress(0, paths.size());
    for (size_t n = 0; n < paths.size(); ++n) {
        const auto& path = paths[n];
        auto bytes = assets.read(path);
        if (path == "ROADRASH.EXE") {
            // The port reads static game tables, not the original executable's
            // machine code, imports, artwork or relocation data. Common legacy
            // compatibility patches must not reject otherwise identical tables.
            auto word = [&](size_t at) -> uint16_t {
                if (at + 2 > bytes.size()) throw std::runtime_error("Invalid PC executable");
                return uint16_t(bytes[at] | (uint16_t(bytes[at + 1]) << 8));
            };
            auto dword = [&](size_t at) -> uint32_t {
                return uint32_t(word(at)) | (uint32_t(word(at + 2)) << 16);
            };
            if (word(0) != 0x5a4d) throw std::runtime_error("Invalid PC executable");
            const size_t pe = dword(0x3c);
            if (dword(pe) != 0x4550) throw std::runtime_error("Invalid PC executable");
            const auto count = word(pe + 6), optional = word(pe + 20);
            if (count > 96) throw std::runtime_error("Invalid PE sections");
            bool found = false;
            for (size_t i = 0; i < count; ++i) {
                const size_t section = pe + 24 + optional + i * 40;
                if (section + 40 > bytes.size()) throw std::runtime_error("Invalid PE section");
                std::string name(reinterpret_cast<const char*>(bytes.data() + section), 8);
                name.resize(name.find('\0') == name.npos ? name.size() : name.find('\0'));
                if (name != ".data" && name != ".rdata") continue;
                const size_t size = dword(section + 16), start = dword(section + 20);
                if (start > bytes.size() || size > bytes.size() - start)
                    throw std::runtime_error("Invalid PE data");
                std::vector<uint8_t> data(bytes.begin() + ptrdiff_t(section + 8),
                                          bytes.begin() + ptrdiff_t(section + 16));
                data.insert(data.end(), bytes.begin() + ptrdiff_t(start),
                            bytes.begin() + ptrdiff_t(start + size));
                rows.emplace_back(path + ":" + name, sha256(data));
                found = true;
            }
            if (!found) throw std::runtime_error("PC executable has no supported static data sections");
        } else
            rows.emplace_back(path, sha256(bytes));
        if (progress) progress(n + 1, paths.size());
    }
    return digest_from_table(rows);
}

DataDigest verified_data_digest(const Assets& assets, const VerificationProgress& progress) {
    std::vector<std::pair<std::string, std::string>> rows;
    const auto& references = reference_hashes();
    if (progress) progress(0, references.size());
    for (const auto& reference : references) {
        if (!assets.exists(reference.path) || sha256(assets.read(reference.path)) != reference.hash)
            throw std::runtime_error("Multiplayer data verification failed: " + reference.path);
        rows.emplace_back(reference.path, reference.hash);
        if (progress) progress(rows.size(), references.size());
    }
    return digest_from_table(rows);
}

Hello make_hello(const std::string& alias, Platform platform, const DataDigest& digest,
                 const std::optional<SeatToken>& seat_token) {
    Hello h;
    h.protocol = protocol_version;
    h.sim_fingerprint = local_fingerprint();
    h.data_digest = digest;
    h.platform = platform;
    h.alias = alias.substr(0, max_alias);
    h.seat_token = seat_token;
    return h;
}

std::string reject_text(RejectReason reason) {
    switch (reason) {
    case RejectReason::protocol:
        return "Version mismatch: update the game and try again.";
    case RejectReason::fingerprint:
        return "Build mismatch: the room was made by a different game version.";
    case RejectReason::data_digest:
        return "Data mismatch: verify the game data and try again.";
    case RejectReason::room_full:
        return "The room is full.";
    case RejectReason::no_such_room:
        return "No room with that code. Check the code or link.";
    case RejectReason::room_racing:
        return "That race already started. Join as a spectator.";
    case RejectReason::rate_limited:
        return "Too many tries. Wait a moment and try again.";
    case RejectReason::server_full:
        return "The server is full. Try again later.";
    case RejectReason::bad_request:
        return "The request failed. Leave the room and try again.";
    case RejectReason::timeout:
        return "The connection timed out. Check the network and retry.";
    case RejectReason::kicked:
        return "Reconnecting from another device.";
    }
    return "The connection failed.";
}

} // namespace rr::net
