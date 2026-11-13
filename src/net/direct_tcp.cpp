#include "direct_tcp.hpp"
#include "room.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <map>
namespace rr::net {
namespace {
constexpr size_t max_packet = 512 * 1024, max_buffer = 4 * 1024 * 1024;
struct Winsock {
    Winsock() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data))
            throw std::runtime_error("Windows TCP/IP initialization failed");
    }
    ~Winsock() { WSACleanup(); }
};
void winsock() {
    static Winsock api;
}
void nonblocking(SOCKET s) {
    u_long one = 1;
    ioctlsocket(s, FIONBIO, &one);
}
void configure(SOCKET s) {
    nonblocking(s);
    BOOL one = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&one), sizeof(one));
}
class TcpChannel final : public Channel {
    std::atomic<ChannelStatus> status_{ChannelStatus::connecting};
    std::atomic<bool> closing_{false};
    std::mutex mutex_;
    std::deque<std::vector<uint8_t>> incoming_, outgoing_;
    size_t incoming_bytes_ = 0, outgoing_bytes_ = 0;
    std::thread worker_;
    void run(SOCKET socket) {
        configure(socket);
        status_ = ChannelStatus::open;
        std::vector<uint8_t> received, pending;
        size_t offset = 0;
        uint64_t close_deadline = 0;
        while (status_ == ChannelStatus::open) {
            if (pending.empty()) {
                std::lock_guard lock(mutex_);
                if (!outgoing_.empty()) {
                    pending = std::move(outgoing_.front());
                    outgoing_.pop_front();
                    outgoing_bytes_ -= pending.size();
                    offset = 0;
                }
            }
            if (closing_) {
                if (!close_deadline) close_deadline = steady_ms() + 200;
                if (pending.empty() || steady_ms() >= close_deadline) break;
            }
            fd_set read{}, write{};
            FD_ZERO(&read);
            FD_ZERO(&write);
            FD_SET(socket, &read);
            if (!pending.empty()) FD_SET(socket, &write);
            timeval timeout{0, 5000};
            const int ready = select(0, &read, &write, nullptr, &timeout);
            if (ready == SOCKET_ERROR) {
                status_ = ChannelStatus::failed;
                break;
            }
            if (FD_ISSET(socket, &write)) {
                const int n = ::send(socket, reinterpret_cast<const char*>(pending.data() + offset),
                                     int(pending.size() - offset), 0);
                if (n > 0) {
                    offset += size_t(n);
                    if (offset == pending.size()) {
                        pending.clear();
                        offset = 0;
                    }
                } else if (n == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
                    status_ = ChannelStatus::failed;
                    break;
                }
            }
            if (FD_ISSET(socket, &read)) {
                uint8_t buffer[8192];
                const int n = recv(socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
                if (n == 0) break;
                if (n < 0) {
                    if (WSAGetLastError() != WSAEWOULDBLOCK) {
                        status_ = ChannelStatus::failed;
                        break;
                    }
                    continue;
                }
                received.insert(received.end(), buffer, buffer + n);
                if (received.size() > max_buffer) {
                    status_ = ChannelStatus::failed;
                    break;
                }
                size_t consumed = 0;
                while (received.size() - consumed >= 4) {
                    uint32_t size = 0;
                    std::memcpy(&size, received.data() + consumed, 4);
                    size = ntohl(size);
                    if (!size || size > max_packet) {
                        status_ = ChannelStatus::failed;
                        break;
                    }
                    if (received.size() - consumed < 4 + size) break;
                    std::lock_guard lock(mutex_);
                    if (incoming_.size() >= 4096 || incoming_bytes_ + size > max_buffer) {
                        status_ = ChannelStatus::failed;
                        break;
                    }
                    incoming_.emplace_back(received.begin() + ptrdiff_t(consumed + 4),
                                           received.begin() + ptrdiff_t(consumed + 4 + size));
                    incoming_bytes_ += size;
                    consumed += 4 + size;
                }
                received.erase(received.begin(), received.begin() + ptrdiff_t(consumed));
            }
        }
        shutdown(socket, SD_BOTH);
        closesocket(socket);
        if (status_ != ChannelStatus::failed) status_ = ChannelStatus::closed;
    }

  public:
    explicit TcpChannel(SOCKET socket) {
        worker_ = std::thread([this, socket] { run(socket); });
    }
    TcpChannel(std::string host, std::string port) {
        winsock();
        worker_ = std::thread([this, host = std::move(host), port = std::move(port)] {
            addrinfo hints{};
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_family = AF_UNSPEC;
            addrinfo* results = nullptr;
            if (getaddrinfo(host.c_str(), port.c_str(), &hints, &results)) {
                status_ = ChannelStatus::failed;
                return;
            }
            SOCKET connected = INVALID_SOCKET;
            for (auto* a = results; a && !closing_; a = a->ai_next) {
                SOCKET s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s == INVALID_SOCKET) continue;
                nonblocking(s);
                int result = connect(s, a->ai_addr, int(a->ai_addrlen));
                if (result == 0) {
                    connected = s;
                    break;
                }
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
                    const auto deadline = steady_ms() + 5000;
                    while (steady_ms() < deadline && !closing_) {
                        fd_set write{}, error{};
                        FD_ZERO(&write);
                        FD_ZERO(&error);
                        FD_SET(s, &write);
                        FD_SET(s, &error);
                        timeval wait{0, 10000};
                        if (select(0, nullptr, &write, &error, &wait) > 0) {
                            int code = 0;
                            int size = sizeof(code);
                            getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&code), &size);
                            if (!code && FD_ISSET(s, &write)) connected = s;
                            break;
                        }
                    }
                }
                if (connected != INVALID_SOCKET) break;
                closesocket(s);
            }
            freeaddrinfo(results);
            if (connected == INVALID_SOCKET) {
                status_ = closing_ ? ChannelStatus::closed : ChannelStatus::failed;
                return;
            }
            run(connected);
        });
    }
    ~TcpChannel() override {
        close();
        if (worker_.joinable()) worker_.join();
    }
    bool reliable() const override { return true; }
    ChannelStatus status() const override { return status_.load(); }
    void close() override { closing_ = true; }
    void send(std::span<const uint8_t> data) override {
        if (closing_ || data.empty() || data.size() > max_packet) {
            status_ = ChannelStatus::failed;
            return;
        }
        std::lock_guard lock(mutex_);
        if (outgoing_bytes_ + data.size() + 4 > max_buffer) {
            status_ = ChannelStatus::failed;
            return;
        }
        std::vector<uint8_t> frame(data.size() + 4);
        uint32_t size = htonl(uint32_t(data.size()));
        std::memcpy(frame.data(), &size, 4);
        std::memcpy(frame.data() + 4, data.data(), data.size());
        outgoing_bytes_ += frame.size();
        outgoing_.push_back(std::move(frame));
    }
    std::optional<std::vector<uint8_t>> poll() override {
        std::lock_guard lock(mutex_);
        if (incoming_.empty()) return {};
        auto frame = std::move(incoming_.front());
        incoming_.pop_front();
        incoming_bytes_ -= frame.size();
        return frame;
    }
};
std::pair<std::string, std::string> endpoint_parts(std::string text) {
    if (text.starts_with("tcp://")) text.erase(0, 6);
    if (text.empty() || text.find_first_of("/\\ \t\r\n") != text.npos)
        throw std::runtime_error("Enter tcp://host:port");
    std::string host, port = std::to_string(direct_default_port);
    if (text.front() == '[') {
        auto end = text.find(']');
        if (end == text.npos) throw std::runtime_error("Invalid IPv6 address");
        host = text.substr(1, end - 1);
        if (end + 1 < text.size()) {
            if (text[end + 1] != ':') throw std::runtime_error("Invalid port");
            port = text.substr(end + 2);
        }
    } else {
        auto colon = text.find(':');
        if (colon != text.npos && colon == text.rfind(':')) {
            host = text.substr(0, colon);
            port = text.substr(colon + 1);
        } else
            host = text;
    }
    if (host.empty() || port.empty() || port.size() > 5 ||
        !std::all_of(port.begin(), port.end(), [](char c) { return c >= '0' && c <= '9'; }) ||
        std::stoi(port) < 1 || std::stoi(port) > 65535)
        throw std::runtime_error("Invalid host or TCP port");
    return {host, port};
}
constexpr char query[] = "RRPORTDISC2";
struct Host {
    SOCKET listener = INVALID_SOCKET, udp = INVALID_SOCKET;
    uint16_t port = 0;
    std::atomic<bool> stop = false;
    std::thread thread;
    netd::Server server;
    explicit Host(uint16_t requested) {
        winsock();
        listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        DWORD no = 0;
        if (listener != INVALID_SOCKET)
            setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&no), sizeof(no));
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(requested);
        if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ||
            listen(listener, 16)) {
            if (listener != INVALID_SOCKET) closesocket(listener);
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_port = htons(requested);
            a.sin_addr.s_addr = INADDR_ANY;
            if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&a), sizeof(a)) ||
                listen(listener, 16)) {
                if (listener != INVALID_SOCKET) closesocket(listener);
                throw std::runtime_error("Cannot host: TCP port is already in use or unavailable");
            }
        }
        sockaddr_storage bound{};
        int size = sizeof(bound);
        getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &size);
        port = ntohs(reinterpret_cast<sockaddr_in*>(&bound)->sin_port);
        nonblocking(listener);
        udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in u{};
        u.sin_family = AF_INET;
        u.sin_port = htons(port);
        u.sin_addr.s_addr = INADDR_ANY;
        if (udp != INVALID_SOCKET) {
            if (bind(udp, reinterpret_cast<sockaddr*>(&u), sizeof(u))) {
                closesocket(udp);
                udp = INVALID_SOCKET;
            } else
                nonblocking(udp);
        }
        thread = std::thread([this] {
            while (!stop) {
                for (int count = 0; count < 16; ++count) {
                    SOCKET s = accept(listener, nullptr, nullptr);
                    if (s == INVALID_SOCKET) break;
                    server.add_connection(std::make_unique<TcpChannel>(s), steady_ms());
                }
                server.step(steady_ms());
                if (udp != INVALID_SOCKET)
                    for (int n = 0; n < 16; ++n) {
                        char buffer[64];
                        sockaddr_in source{};
                        int len = sizeof(source);
                        const int count = recvfrom(udp, buffer, sizeof(buffer), 0,
                                                   reinterpret_cast<sockaddr*>(&source), &len);
                        if (count < 0) break;
                        if (count != int(sizeof(query) - 1 + 8) ||
                            std::memcmp(buffer, query, sizeof(query) - 1))
                            continue;
                        auto encoded = encode(server.public_room_list());
                        if (encoded.size() > 1200) continue;
                        std::vector<uint8_t> response{'R', 'R', 'D', '2'};
                        response.insert(response.end(), buffer + sizeof(query) - 1, buffer + count);
                        response.push_back(uint8_t(port >> 8));
                        response.push_back(uint8_t(port));
                        response.insert(response.end(), encoded.begin(), encoded.end());
                        sendto(udp, reinterpret_cast<const char*>(response.data()), int(response.size()), 0,
                               reinterpret_cast<sockaddr*>(&source), sizeof(source));
                    }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    ~Host() {
        stop = true;
        if (thread.joinable()) thread.join();
        if (udp != INVALID_SOCKET) closesocket(udp);
        if (listener != INVALID_SOCKET) closesocket(listener);
    }
};
std::unique_ptr<Host>& host() {
    static std::unique_ptr<Host> value;
    return value;
}
struct Discovery {
    SOCKET socket = INVALID_SOCKET;
    uint16_t port = direct_default_port;
    uint64_t nonce = 0, next_query = 0;
    struct Entry {
        RoomListEntry room;
        std::string endpoint;
        uint64_t seen = 0;
    };
    std::map<std::string, Entry> entries;
    Discovery() {
        winsock();
        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket != INVALID_SOCKET) {
            BOOL yes = TRUE;
            setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&yes), sizeof(yes));
            nonblocking(socket);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = INADDR_ANY;
            bind(socket, reinterpret_cast<sockaddr*>(&a), sizeof(a));
        }
        nonce = (uint64_t(std::random_device{}()) << 32) ^ std::random_device{}();
    }
    ~Discovery() {
        if (socket != INVALID_SOCKET) closesocket(socket);
    }
    void poll() {
        const auto now = steady_ms();
        if (socket == INVALID_SOCKET) return;
        if (now >= next_query) {
            std::vector<uint8_t> request(query, query + sizeof(query) - 1);
            const auto* p = reinterpret_cast<const uint8_t*>(&nonce);
            request.insert(request.end(), p, p + 8);
            for (uint32_t address : {uint32_t(INADDR_BROADCAST), uint32_t(INADDR_LOOPBACK)}) {
                sockaddr_in to{};
                to.sin_family = AF_INET;
                to.sin_port = htons(port);
                to.sin_addr.s_addr = htonl(address);
                sendto(socket, reinterpret_cast<const char*>(request.data()), int(request.size()), 0,
                       reinterpret_cast<sockaddr*>(&to), sizeof(to));
            }
            next_query = now + 1000;
        }
        for (int i = 0; i < 32; ++i) {
            uint8_t buffer[1400];
            sockaddr_in from{};
            int size = sizeof(from);
            int count = recvfrom(socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                 reinterpret_cast<sockaddr*>(&from), &size);
            if (count < 0) break;
            if (count < 14 || std::memcmp(buffer, "RRD2", 4) || std::memcmp(buffer + 4, &nonce, 8)) continue;
            auto decoded = decode(std::span<const uint8_t>(buffer + 14, size_t(count - 14)));
            if (!decoded) continue;
            auto* list = std::get_if<RoomListReply>(&*decoded);
            if (!list) continue;
            char address[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &from.sin_addr, address, sizeof(address));
            const auto endpoint = "tcp://" + std::string(address) + ":" +
                                  std::to_string((unsigned(buffer[12]) << 8) | buffer[13]);
            for (auto room : list->rooms) {
                const auto code = room.code;
                room.host += " [" + std::string(address) + "]";
                entries[code] = {std::move(room), endpoint, now};
            }
        }
        for (auto i = entries.begin(); i != entries.end();)
            if (now - i->second.seen > 5000)
                i = entries.erase(i);
            else
                ++i;
    }
};
Discovery& discovery() {
    static Discovery d;
    return d;
}
} // namespace
std::unique_ptr<Channel> direct_connect(const std::string& endpoint) {
    auto [address, port] = endpoint_parts(endpoint);
    return std::make_unique<TcpChannel>(address, port);
}
uint16_t direct_start_host(uint16_t port) {
    winsock();
    auto& h = host();
    if (!h) h = std::make_unique<Host>(port);
    return h->port;
}
void direct_stop_host() {
    host().reset();
}
void direct_refresh_discovery(uint16_t port) {
    auto& d = discovery();
    d.port = port;
    d.entries.clear();
    d.next_query = 0;
    d.poll();
}
std::vector<RoomListEntry> direct_discovered_rooms() {
    auto& d = discovery();
    d.poll();
    std::vector<RoomListEntry> out;
    for (const auto& [code, e] : d.entries) out.push_back(e.room);
    return out;
}
std::string direct_room_endpoint(const std::string& code) {
    auto& d = discovery();
    d.poll();
    auto i = d.entries.find(code);
    return i == d.entries.end() ? std::string{} : i->second.endpoint;
}
std::optional<std::pair<std::string, std::string>> direct_parse_invite(const std::string& text) {
    const auto slash = text.rfind('/');
    if (slash == text.npos) return {};
    auto code = parse_room_code(text.substr(slash + 1));
    if (!code) return {};
    auto endpoint = text.substr(0, slash);
    try {
        endpoint_parts(endpoint);
    } catch (...) {
        return {};
    }
    return std::pair(endpoint, *code);
}
std::string direct_invite(const std::string& code) {
    winsock();
    std::string address = "127.0.0.1";
    char name[256]{};
    if (!gethostname(name, sizeof(name))) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* list = nullptr;
        if (!getaddrinfo(name, nullptr, &hints, &list)) {
            for (auto* a = list; a; a = a->ai_next) {
                auto* ip = reinterpret_cast<sockaddr_in*>(a->ai_addr);
                if ((ntohl(ip->sin_addr.s_addr) >> 24) == 127) continue;
                char text[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &ip->sin_addr, text, sizeof(text));
                address = text;
                break;
            }
            freeaddrinfo(list);
        }
    }
    const auto port = host() ? host()->port : direct_default_port;
    return "tcp://" + address + ":" + std::to_string(port) + "/" + code;
}
} // namespace rr::net
