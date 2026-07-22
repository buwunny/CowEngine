// Headless authoritative server entry point.
//
// Wire framing on the server<->sidecar UDP hop (the sidecar owns the browser's
// WebTransport/WebSocket connection and multiplexes them onto this one socket):
//
//   [u32 session][u8 kind][payload...]
//     kind 0 = connect      (no payload)
//     kind 1 = disconnect   (no payload)
//     kind 2 = message on the unreliable channel (payload = encoded net::Message)
//     kind 3 = message on the reliable channel   (payload = encoded net::Message)
//
// The server replies with the same framing; the sidecar routes each reply back
// to the browser identified by `session`.

#include "server/GameServer.hpp"
#include "net/UdpLink.hpp"
#include "net/Protocol.hpp"
#include "net/ByteIO.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

namespace
{
    enum FrameKind : uint8_t
    {
        FrameConnect = 0,
        FrameDisconnect = 1,
        FrameUnreliable = 2,
        FrameReliable = 3,
    };
}

int main(int argc, char **argv)
{
    uint16_t port = 4433;
    std::string scenePath = "scenes/scene.json";
    if (argc > 1)
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc > 2)
        scenePath = argv[2];

    net::UdpLink udp;
    if (!udp.bind(port))
        return 1;

    // sockaddr for each live session, refreshed on every inbound datagram so
    // replies always go to the sidecar's current source address.
    std::unordered_map<uint32_t, sockaddr_in> sessionAddr;

    GameServer server;
    if (!server.init(scenePath))
        return 1;

    server.setSend([&](uint32_t session, const net::Message &m) {
        auto it = sessionAddr.find(session);
        if (it == sessionAddr.end())
            return;
        std::vector<uint8_t> payload = net::encode(m);
        net::ByteWriter w;
        w.u32(session);
        w.u8(net::channelFor(net::typeOf(m)) == net::Channel::Unreliable
                 ? FrameUnreliable
                 : FrameReliable);
        w.buf.insert(w.buf.end(), payload.begin(), payload.end());
        udp.sendTo(w.buf.data(), w.buf.size(), it->second);
    });

    std::cout << "CowEngine server listening on udp/" << port
              << " (scene: " << scenePath << ")\n";

    using clock = std::chrono::steady_clock;
    const double kFixedDt = 1.0 / 60.0;
    auto prev = clock::now();
    double accumulator = 0.0;

    for (;;)
    {
        // 1) Drain all pending datagrams.
        std::vector<uint8_t> data;
        sockaddr_in from{};
        while (udp.recv(data, from))
        {
            net::ByteReader r(data.data(), data.size());
            uint32_t session = r.u32();
            uint8_t kind = r.u8();
            if (!r.ok)
                continue;
            sessionAddr[session] = from;

            switch (kind)
            {
            case FrameConnect:
                server.onConnect(session);
                break;
            case FrameDisconnect:
                server.onDisconnect(session);
                sessionAddr.erase(session);
                break;
            case FrameUnreliable:
            case FrameReliable:
            {
                // Payload begins after the 5-byte header.
                if (data.size() > 5)
                {
                    if (auto msg = net::decode(data.data() + 5, data.size() - 5))
                        server.onMessage(session, *msg);
                }
                break;
            }
            default:
                break;
            }
        }

        // 2) Advance the simulation in fixed steps.
        auto now = clock::now();
        accumulator += std::chrono::duration<double>(now - prev).count();
        prev = now;
        if (accumulator > 0.25)
            accumulator = 0.25;
        while (accumulator >= kFixedDt)
        {
            server.tick(static_cast<float>(kFixedDt));
            accumulator -= kFixedDt;
        }

        // 3) Yield so we don't spin a core at 100%.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
