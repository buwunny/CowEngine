#ifndef NET_WEB_CLIENT_TRANSPORT_HPP
#define NET_WEB_CLIENT_TRANSPORT_HPP

#include "net/ITransport.hpp"

#include <deque>
#include <string>

// Browser-side ITransport for the WASM client. It is a thin bridge to the
// `window.CowNet` JavaScript shim (see GameTemplate.html), which owns the actual
// WebTransport (primary) or WebSocket (fallback) connection. The WASM build has
// no threads/Asyncify, so inbound messages are pushed from JS into an in-memory
// queue via the exported `cow_net_deliver`, and the game loop drains that queue
// with poll() once per tick.
namespace net
{
    class WebClientTransport : public ITransport
    {
    public:
        WebClientTransport();
        ~WebClientTransport() override;

        // Whether a multiplayer server is configured (window.CowNet resolved a
        // wt/ws URL from __COWENGINE_SERVER__ or the ?wt/?ws query params).
        static bool serverConfigured();

        // Begin connecting, using the config window.CowNet already resolved.
        void connect();

        void sendUnreliable(const uint8_t *data, size_t len) override;
        void sendReliable(const uint8_t *data, size_t len) override;
        bool poll(Incoming &out) override;
        TransportState state() const override;

        // Called from JS (via the exported cow_net_deliver) with one inbound
        // message. Copies the bytes into the queue.
        void deliver(uint8_t channel, const uint8_t *data, size_t len);

    private:
        std::deque<Incoming> inbox_;
    };
}

#endif // NET_WEB_CLIENT_TRANSPORT_HPP
