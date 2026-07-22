#include "net/UdpLink.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net
{
    UdpLink::~UdpLink() { close(); }

    bool UdpLink::bind(uint16_t port)
    {
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0)
        {
            std::cerr << "UdpLink: socket() failed: " << std::strerror(errno) << "\n";
            return false;
        }

        int flags = ::fcntl(fd_, F_GETFL, 0);
        ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            std::cerr << "UdpLink: bind(" << port << ") failed: " << std::strerror(errno) << "\n";
            close();
            return false;
        }
        return true;
    }

    void UdpLink::close()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool UdpLink::recv(std::vector<uint8_t> &out, sockaddr_in &from)
    {
        if (fd_ < 0)
            return false;
        uint8_t buf[2048];
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(fd_, buf, sizeof(buf), 0,
                               reinterpret_cast<sockaddr *>(&from), &fromLen);
        if (n <= 0)
            return false; // EWOULDBLOCK / no datagram this poll
        out.assign(buf, buf + n);
        return true;
    }

    void UdpLink::sendTo(const uint8_t *data, size_t len, const sockaddr_in &to)
    {
        if (fd_ < 0)
            return;
        ::sendto(fd_, data, len, 0, reinterpret_cast<const sockaddr *>(&to), sizeof(to));
    }
}
