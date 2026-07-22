#ifndef NET_ITRANSPORT_HPP
#define NET_ITRANSPORT_HPP

#include "net/Protocol.hpp"

#include <cstdint>
#include <vector>

// The single seam that both client-side transports (WebTransport, WebSocket,
// native UDP for dev) implement. It is intentionally non-blocking: the game
// loop drains inbound messages with poll() once per tick (the WASM build has no
// threads/Asyncify) and hands outbound bytes off with send*(). Framing and
// reliability live in the concrete transport; this interface speaks whole
// messages.
namespace net
{
    enum class TransportState : uint8_t
    {
        Disconnected,
        Connecting,
        Connected,
        Failed,
    };

    // One message pulled off the wire, tagged with the channel it arrived on.
    struct Incoming
    {
        Channel channel = Channel::Reliable;
        std::vector<uint8_t> bytes;
    };

    class ITransport
    {
    public:
        virtual ~ITransport() = default;

        virtual void sendUnreliable(const uint8_t *data, size_t len) = 0;
        virtual void sendReliable(const uint8_t *data, size_t len) = 0;

        // Pop the next inbound message into `out`; returns false when the queue
        // is empty this tick. Call in a loop until it returns false.
        virtual bool poll(Incoming &out) = 0;

        virtual TransportState state() const = 0;
        bool connected() const { return state() == TransportState::Connected; }

        // Convenience: encode + route a Message onto the channel its type wants.
        void send(const Message &m)
        {
            std::vector<uint8_t> bytes = encode(m);
            if (channelFor(typeOf(m)) == Channel::Unreliable)
                sendUnreliable(bytes.data(), bytes.size());
            else
                sendReliable(bytes.data(), bytes.size());
        }
    };
}

#endif // NET_ITRANSPORT_HPP
