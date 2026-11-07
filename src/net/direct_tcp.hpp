#pragma once
#include "channel.hpp"
#include "protocol.hpp"
namespace rr::net {
constexpr uint16_t direct_default_port = 27995;
std::unique_ptr<Channel> direct_connect(const std::string& endpoint);
// Hosting owns a small room/sealer server inside the game; no separate daemon.
uint16_t direct_start_host(uint16_t port = direct_default_port);
void direct_stop_host();
void direct_refresh_discovery(uint16_t port = direct_default_port);
std::vector<RoomListEntry> direct_discovered_rooms();
std::string direct_room_endpoint(const std::string& code);
std::optional<std::pair<std::string, std::string>> direct_parse_invite(const std::string&);
std::string direct_invite(const std::string& code);
} // namespace rr::net
