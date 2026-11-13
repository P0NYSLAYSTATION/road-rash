// Plan 34 Task 5 Step 1: rr-netd room logic driven by in-memory channels and
// a virtual clock: create/join/list, rejections, picks, votes, chat, start,
// host hand-over, reconnect by seat token, timeouts, race frames, results,
// rate limiting and malformed input.
#include "check.hpp"
#include "net/channel.hpp"
#include "net/protocol.hpp"
#include "room.hpp"

#include <deque>
#include <map>

using namespace rr::net;
using rr::netd::Server;
using rr::netd::ServerConfig;

namespace {

uint64_t now = 0;
const Clock clock_fn = [] { return now; };

struct Client {
    std::unique_ptr<Channel> ch;
    std::deque<Message> inbox;
    void send(const Message& m) { ch->send(encode(m)); }
    void raw(std::vector<uint8_t> b) { ch->send(b); }
    void drain() {
        while (auto p = ch->poll())
            if (auto m = decode(*p)) inbox.push_back(*m);
    }
    template <class T> std::optional<T> last() {
        drain();
        std::optional<T> out;
        for (const Message& m : inbox)
            if (const T* t = std::get_if<T>(&m)) out = *t;
        return out;
    }
    template <class T> std::vector<T> all() {
        drain();
        std::vector<T> out;
        for (const Message& m : inbox)
            if (const T* t = std::get_if<T>(&m)) out.push_back(*t);
        return out;
    }
    void clear() {
        drain();
        inbox.clear();
    }
};

Hello hello(const std::string& alias, uint64_t fp = 0x1234) {
    Hello h;
    h.alias = alias;
    h.sim_fingerprint = fp;
    h.data_digest[0] = 0xab;
    h.platform = Platform::bot;
    return h;
}

Client connect(Server& s) {
    auto [a, b] = loopback_pair({0, 0, 0, 0, 1}, true, clock_fn);
    s.add_connection(std::move(b), now);
    return Client{std::move(a), {}};
}

void run(Server& s, uint64_t ms, uint64_t step = 4) {
    const uint64_t end = now + ms;
    while (now < end) {
        now += std::min(step, end - now);
        s.step(now);
    }
}

} // namespace

int main() {
    rr::test::Checks check;
    ServerConfig cfg;cfg.seed=42;cfg.min_humans=2;cfg.load_ms=10;
    Server server(cfg);
    Client host=connect(server),peer=connect(server);
    host.send(hello("Host"));host.send(RoomCreate{{0,2,true,true,true}});run(server,20);
    const auto welcome=host.last<Welcome>();
    if(!check(welcome.has_value(),"snapshot room created"))return check.finish("snapshot_server");
    peer.send(hello("Peer"));peer.send(RoomJoin{welcome->room_code,false});run(server,20);
    host.send(PickCharacter{1});peer.send(PickCharacter{2});
    host.send(VoteCourse{4});peer.send(VoteCourse{4});run(server,20);
    auto start=host.last<Start>();
    if(!check(start.has_value(),"snapshot race starts"))return check.finish("snapshot_server");
    check(start->options.snapshots&&start->authority_seat==0,"host elected as snapshot authority");
    const uint32_t race=start->race_id;
    host.clear();peer.clear();
    SnapshotState state{race,2,{0,5},{1,3},{0,1,2}};
    peer.send(state);
    peer.send(SnapshotResults{race,2,{{0,Outcome::busted,0},{1,Outcome::won,1}}});
    peer.send(SnapshotControls{race,5,3,0});run(server,20);
    check(!host.last<SnapshotState>()&&!host.last<Official>(),"peer cannot publish authoritative world or result");
    auto relayed=host.last<SnapshotControls>();
    check(relayed&&relayed->seat==1&&relayed->sequence==5&&relayed->controls==3,
          "control seat is authenticated from connection, ignoring claimed host seat");
    check(!peer.last<SnapshotControls>(),"controls sent only to authority");
    host.send(state);run(server,20);
    check(peer.last<SnapshotState>()==state,"authoritative state relayed unchanged");
    host.clear();peer.clear();
    peer.send(SnapshotControls{race,4,7,0});run(server,20);
    check(!host.last<SnapshotControls>(),"stale control sequence not replayed");
    host.send(SnapshotState{race,1,{},{},{9}});run(server,20);
    check(!peer.last<SnapshotState>(),"stale authoritative tick rejected");
    Client spectator=connect(server);spectator.send(hello("Viewer"));
    spectator.send(RoomJoin{welcome->room_code,true});run(server,20);
    check(spectator.last<Start>()&&spectator.last<Start>()->your_seat==0xff,"late spectator gets race start");
    check(spectator.last<SnapshotState>()==state,"late spectator gets latest complete snapshot");
    host.clear();spectator.send(SnapshotControls{race,6,1,0});run(server,20);
    check(!host.last<SnapshotControls>(),"spectator cannot submit controls");
    host.send(SnapshotResults{race,100,{{0,Outcome::won,1},{1,Outcome::finished,2}}});run(server,20);
    auto official=peer.last<Official>();
    check(official&&official->rows.size()==2&&official->rows[0].outcome==Outcome::won,
          "only host publishes official race outcome");
    host.clear();peer.clear();host.send(VoteCourse{4});peer.send(VoteCourse{4});run(server,20);
    check(host.last<Start>().has_value(),"second snapshot race starts");
    host.ch->close();run(server,20);
    check(peer.last<Official>().has_value(),"authority disconnect ends race instead of silently changing authority");
    check(server.counters().invalid>=3,"forged state/results and stale state are counted");
    return check.finish("snapshot_server");
}
