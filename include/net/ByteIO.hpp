#ifndef NET_BYTE_IO_HPP
#define NET_BYTE_IO_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Compact little-endian binary (de)serialisation for the networking hot path.
// Both ends of the wire (browser WASM, x86/ARM server) are little-endian, but
// we encode explicitly so the format never depends on host byte order.
namespace net
{
    class ByteWriter
    {
    public:
        std::vector<uint8_t> buf;

        void u8(uint8_t v) { buf.push_back(v); }
        void u16(uint16_t v)
        {
            buf.push_back(uint8_t(v & 0xFF));
            buf.push_back(uint8_t((v >> 8) & 0xFF));
        }
        void u32(uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                buf.push_back(uint8_t((v >> (8 * i)) & 0xFF));
        }
        void u64(uint64_t v)
        {
            for (int i = 0; i < 8; ++i)
                buf.push_back(uint8_t((v >> (8 * i)) & 0xFF));
        }
        void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
        void f32(float v)
        {
            uint32_t bits;
            std::memcpy(&bits, &v, 4);
            u32(bits);
        }
        void vec3(const glm::vec3 &v)
        {
            f32(v.x);
            f32(v.y);
            f32(v.z);
        }
        void quat(const glm::quat &q)
        {
            f32(q.x);
            f32(q.y);
            f32(q.z);
            f32(q.w);
        }
        // Length-prefixed (u16) UTF-8 string.
        void str(const std::string &s)
        {
            u16(static_cast<uint16_t>(s.size()));
            buf.insert(buf.end(), s.begin(), s.end());
        }
    };

    // Cursor over a fixed buffer. Any read past the end sets `ok = false`; all
    // subsequent reads return zero-values so callers can check `ok` once at the
    // end rather than after every field.
    class ByteReader
    {
    public:
        ByteReader(const uint8_t *data, size_t len) : p_(data), end_(data + len) {}

        bool ok = true;
        bool atEnd() const { return p_ >= end_; }

        uint8_t u8()
        {
            if (!take(1))
                return 0;
            return *(p_++);
        }
        uint16_t u16()
        {
            if (!take(2))
                return 0;
            uint16_t v = uint16_t(p_[0]) | (uint16_t(p_[1]) << 8);
            p_ += 2;
            return v;
        }
        uint32_t u32()
        {
            if (!take(4))
                return 0;
            uint32_t v = 0;
            for (int i = 0; i < 4; ++i)
                v |= uint32_t(p_[i]) << (8 * i);
            p_ += 4;
            return v;
        }
        uint64_t u64()
        {
            if (!take(8))
                return 0;
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
                v |= uint64_t(p_[i]) << (8 * i);
            p_ += 8;
            return v;
        }
        int32_t i32() { return static_cast<int32_t>(u32()); }
        float f32()
        {
            uint32_t bits = u32();
            float v;
            std::memcpy(&v, &bits, 4);
            return v;
        }
        glm::vec3 vec3()
        {
            float x = f32(), y = f32(), z = f32();
            return {x, y, z};
        }
        glm::quat quat()
        {
            float x = f32(), y = f32(), z = f32(), w = f32();
            return glm::quat(w, x, y, z); // glm::quat ctor is (w, x, y, z)
        }
        std::string str()
        {
            uint16_t n = u16();
            if (!take(n))
                return {};
            std::string s(reinterpret_cast<const char *>(p_), n);
            p_ += n;
            return s;
        }

    private:
        bool take(size_t n)
        {
            if (!ok || size_t(end_ - p_) < n)
            {
                ok = false;
                return false;
            }
            return true;
        }
        const uint8_t *p_;
        const uint8_t *end_;
    };
}

#endif // NET_BYTE_IO_HPP
