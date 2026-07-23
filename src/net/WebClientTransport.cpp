#include "net/WebClientTransport.hpp"

#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// --- JS bridge (implemented by window.CowNet in GameTemplate.html) ----------

EM_JS(void, cow_js_connect, (), {
    if (!window.CowNet)
    {
        console.error("CowNet shim missing");
        return;
    }
    window.CowNet.connect(); // uses the config CowNet already resolved
});

EM_JS(int, cow_js_has_server, (), {
    return (window.CowNet && window.CowNet.hasServer()) ? 1 : 0;
});

// Returns a malloc'd UTF-8 copy of the configured display name (the caller
// frees it). Strings can't cross the EM_JS boundary any other way.
EM_JS(char *, cow_js_player_name, (), {
    var n = (window.CowNet && window.CowNet.config && window.CowNet.config.name)
        ? String(window.CowNet.config.name) : "";
    var len = lengthBytesUTF8(n) + 1;
    var p = _malloc(len);
    stringToUTF8(n, p, len);
    return p;
});

EM_JS(int, cow_js_state, (), {
    return window.CowNet ? window.CowNet.stateCode() : 0;
});

EM_JS(void, cow_js_send, (int channel, const uint8_t *ptr, int len), {
    if (window.CowNet)
        window.CowNet.send(channel, HEAPU8.subarray(ptr, ptr + len));
});
#endif // __EMSCRIPTEN__

namespace net
{
    namespace
    {
        // The transport JS delivers into. There is one client connection, so a
        // single active pointer mirrors the app-global pattern used elsewhere.
        WebClientTransport *g_active = nullptr;
    }

    WebClientTransport::WebClientTransport() { g_active = this; }
    WebClientTransport::~WebClientTransport()
    {
        if (g_active == this)
            g_active = nullptr;
    }

    bool WebClientTransport::serverConfigured()
    {
#ifdef __EMSCRIPTEN__
        return cow_js_has_server() != 0;
#else
        return false;
#endif
    }

    std::string WebClientTransport::playerName()
    {
#ifdef __EMSCRIPTEN__
        char *p = cow_js_player_name();
        if (!p)
            return {};
        std::string name(p);
        std::free(p);
        return name;
#else
        return {};
#endif
    }

    void WebClientTransport::connect()
    {
#ifdef __EMSCRIPTEN__
        cow_js_connect();
#endif
    }

    void WebClientTransport::sendUnreliable(const uint8_t *data, size_t len)
    {
#ifdef __EMSCRIPTEN__
        cow_js_send(0, data, static_cast<int>(len));
#else
        (void)data;
        (void)len;
#endif
    }

    void WebClientTransport::sendReliable(const uint8_t *data, size_t len)
    {
#ifdef __EMSCRIPTEN__
        cow_js_send(1, data, static_cast<int>(len));
#else
        (void)data;
        (void)len;
#endif
    }

    bool WebClientTransport::poll(Incoming &out)
    {
        if (inbox_.empty())
            return false;
        out = std::move(inbox_.front());
        inbox_.pop_front();
        return true;
    }

    TransportState WebClientTransport::state() const
    {
#ifdef __EMSCRIPTEN__
        switch (cow_js_state())
        {
        case 1:
            return TransportState::Connecting;
        case 2:
            return TransportState::Connected;
        case 3:
            return TransportState::Failed;
        default:
            return TransportState::Disconnected;
        }
#else
        return TransportState::Disconnected;
#endif
    }

    void WebClientTransport::deliver(uint8_t channel, const uint8_t *data, size_t len)
    {
        Incoming in;
        in.channel = channel == 1 ? Channel::Reliable : Channel::Unreliable;
        in.bytes.assign(data, data + len);
        inbox_.push_back(std::move(in));
    }
}

#ifdef __EMSCRIPTEN__
// Invoked from JS (window.CowNet) with one inbound message: malloc a buffer,
// copy the bytes into HEAPU8, call this, then free. We copy again into the
// queue, so the JS-side buffer can be released immediately after.
extern "C" EMSCRIPTEN_KEEPALIVE void cow_net_deliver(int channel, uint8_t *data, int len)
{
    if (net::g_active)
        net::g_active->deliver(static_cast<uint8_t>(channel), data, static_cast<size_t>(len));
}
#endif
