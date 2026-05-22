#ifndef EMBEDDED_TEMPLATE_HPP
#define EMBEDDED_TEMPLATE_HPP

#include <cstddef>
#include <vector>
#include <string>

// One file embedded into the editor binary at build time.
struct EmbeddedFile
{
    const char *relativePath;
    const unsigned char *data;
    std::size_t size;
};

// Logical slice of the embedded archive corresponding to a build target.
// Not all slices are populated on every build (e.g. a Linux editor build
// only contains NativeLinux and Web). `EmbeddedTemplate_hasSlice` reports
// whether bytes are actually present for a given slice.
enum class EmbeddedSlice
{
    NativeLinux,
    NativeWindows,
    Web,
};

bool        EmbeddedTemplate_hasSlice(EmbeddedSlice s);
std::size_t EmbeddedTemplate_fileCount(EmbeddedSlice s);
EmbeddedFile EmbeddedTemplate_fileAt(EmbeddedSlice s, std::size_t i);

// In-memory file staged for archival (used by both web and native paths
// before final write/zip).
struct StagedFile
{
    std::string path;
    std::vector<unsigned char> bytes;
};

#if defined(__EMSCRIPTEN__)
// Hand a flat list of files to JS, which zips them and triggers a browser
// download. Implementation lives next to the EM_JS block.
void EmbeddedTemplate_jsDownloadZip(const std::vector<StagedFile> &files,
                                    const std::string &zipName);
#endif

#endif // EMBEDDED_TEMPLATE_HPP
