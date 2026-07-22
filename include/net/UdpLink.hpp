#ifndef NET_UDP_LINK_HPP
#define NET_UDP_LINK_HPP

#include <cstdint>
#include <vector>

#include <netinet/in.h>

// Minimal non-blocking UDP socket for the server<->sidecar hop. The server
// binds a loopback port; the sidecar (or, in tests, a fake client) sends framed
// datagrams and the server replies to the sender address. Non-blocking so the
// fixed-tick main loop can drain it without stalling.
namespace net
{
    class UdpLink
    {
    public:
        UdpLink() = default;
        ~UdpLink();

        bool bind(uint16_t port);
        void close();

        // Pop one datagram into `out`, with its sender in `from`. Returns false
        // when nothing is waiting (call in a loop until false).
        bool recv(std::vector<uint8_t> &out, sockaddr_in &from);
        void sendTo(const uint8_t *data, size_t len, const sockaddr_in &to);

        bool valid() const { return fd_ >= 0; }

    private:
        int fd_ = -1;
    };
}

#endif // NET_UDP_LINK_HPP
