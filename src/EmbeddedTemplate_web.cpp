// Web build of the embedded-template interface.
//
// The web editor mounts the game-web template into MEMFS at /game_template_web/
// via Emscripten --preload-file. At first use we scan that path once and cache
// the results. Native slices are always empty (the web editor cannot produce
// Linux/Windows binaries).

#include "EmbeddedTemplate.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <emscripten.h>

namespace
{
struct CachedFile
{
    std::string relPath;
    std::vector<unsigned char> bytes;
};

struct Cache
{
    std::vector<CachedFile> webFiles;
    bool loaded = false;
} g_cache;
std::once_flag g_loadOnce;

void loadWebSlice()
{
    namespace fs = std::filesystem;
    const fs::path root = "/game_template_web";
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    for (auto &entry : fs::recursive_directory_iterator(root, ec))
    {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        auto rel = fs::relative(entry.path(), root, ec);
        std::ifstream f(entry.path(), std::ios::binary);
        if (!f) continue;
        f.seekg(0, std::ios::end);
        auto sz = f.tellg();
        f.seekg(0, std::ios::beg);
        CachedFile cf;
        cf.relPath = rel.generic_string();
        cf.bytes.resize(static_cast<size_t>(sz));
        if (sz > 0) f.read(reinterpret_cast<char *>(cf.bytes.data()), sz);
        g_cache.webFiles.push_back(std::move(cf));
    }
    g_cache.loaded = true;
}

void ensureLoaded()
{
    std::call_once(g_loadOnce, loadWebSlice);
}
} // namespace

bool EmbeddedTemplate_hasSlice(EmbeddedSlice s)
{
    ensureLoaded();
    if (s == EmbeddedSlice::Web) return !g_cache.webFiles.empty();
    return false; // web editor never carries native binaries
}

std::size_t EmbeddedTemplate_fileCount(EmbeddedSlice s)
{
    ensureLoaded();
    if (s == EmbeddedSlice::Web) return g_cache.webFiles.size();
    return 0;
}

EmbeddedFile EmbeddedTemplate_fileAt(EmbeddedSlice s, std::size_t i)
{
    ensureLoaded();
    if (s == EmbeddedSlice::Web && i < g_cache.webFiles.size())
    {
        const auto &cf = g_cache.webFiles[i];
        return EmbeddedFile{cf.relPath.c_str(), cf.bytes.data(), cf.bytes.size()};
    }
    return EmbeddedFile{nullptr, nullptr, 0};
}

// ----- JS-side zip + download -----------------------------------------------
//
// We build a minimal STORED (uncompressed) zip in C++ — zero deps, ~50 lines.
// The bytes are passed to JS which wraps them in a Blob and triggers a
// browser download.

namespace
{
// CRC32 (zip uses IEEE polynomial).
uint32_t crc32(const unsigned char *data, std::size_t n)
{
    static uint32_t table[256];
    static bool init = false;
    if (!init)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i)
        c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void put16(std::vector<unsigned char> &v, uint16_t x)
{
    v.push_back(uint8_t(x & 0xFF));
    v.push_back(uint8_t((x >> 8) & 0xFF));
}
void put32(std::vector<unsigned char> &v, uint32_t x)
{
    v.push_back(uint8_t(x & 0xFF));
    v.push_back(uint8_t((x >> 8) & 0xFF));
    v.push_back(uint8_t((x >> 16) & 0xFF));
    v.push_back(uint8_t((x >> 24) & 0xFF));
}

std::vector<unsigned char> buildStoredZip(const std::vector<StagedFile> &files)
{
    // Drop duplicate entries (later writes win — used for scene snapshot overlay)
    std::vector<const StagedFile *> kept;
    for (const auto &f : files)
    {
        bool replaced = false;
        for (auto &k : kept)
        {
            if (k->path == f.path) { k = &f; replaced = true; break; }
        }
        if (!replaced) kept.push_back(&f);
    }

    std::vector<unsigned char> out;
    struct Central { std::string name; uint32_t crc; uint32_t size; uint32_t offset; };
    std::vector<Central> central;

    for (auto *fp : kept)
    {
        const auto &name = fp->path;
        const auto &data = fp->bytes;
        uint32_t crc = crc32(data.data(), data.size());
        uint32_t offset = static_cast<uint32_t>(out.size());

        // Local file header (signature 0x04034b50)
        put32(out, 0x04034b50);
        put16(out, 20);            // version needed
        put16(out, 0);             // flags
        put16(out, 0);             // method = STORED
        put16(out, 0);             // mod time
        put16(out, 0);             // mod date
        put32(out, crc);
        put32(out, static_cast<uint32_t>(data.size())); // compressed size
        put32(out, static_cast<uint32_t>(data.size())); // uncompressed size
        put16(out, static_cast<uint16_t>(name.size()));
        put16(out, 0);             // extra len
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());

        central.push_back({name, crc, static_cast<uint32_t>(data.size()), offset});
    }

    uint32_t cdStart = static_cast<uint32_t>(out.size());
    for (auto &c : central)
    {
        // Central directory header (signature 0x02014b50)
        put32(out, 0x02014b50);
        put16(out, 20);            // version made by
        put16(out, 20);            // version needed
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, c.crc);
        put32(out, c.size);
        put32(out, c.size);
        put16(out, static_cast<uint16_t>(c.name.size()));
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, c.offset);
        out.insert(out.end(), c.name.begin(), c.name.end());
    }
    uint32_t cdSize = static_cast<uint32_t>(out.size() - cdStart);

    // End of central directory (signature 0x06054b50)
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<uint16_t>(central.size()));
    put16(out, static_cast<uint16_t>(central.size()));
    put32(out, cdSize);
    put32(out, cdStart);
    put16(out, 0);

    return out;
}
} // namespace

// Hand `bytes` to JS as a browser download. Pure JS side — no DOM deps from C++.
EM_JS(void, em_trigger_download, (const char *filename, const unsigned char *data, size_t size), {
    var bytes = HEAPU8.slice(data, data + size);
    var blob = new Blob([bytes], { type: 'application/zip' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = UTF8ToString(filename);
    document.body.appendChild(a);
    a.click();
    setTimeout(function() { document.body.removeChild(a); URL.revokeObjectURL(url); }, 0);
});

void EmbeddedTemplate_jsDownloadZip(const std::vector<StagedFile> &files,
                                    const std::string &zipName)
{
    auto bytes = buildStoredZip(files);
    em_trigger_download(zipName.c_str(), bytes.data(), bytes.size());
}
