#include "render/TextRenderer.hpp"

#include "render/Shader.hpp"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

// Keep every stb symbol internal to this translation unit: the editor build
// also links Dear ImGui, which carries its own copy of stb_truetype.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

namespace
{
    // Slack baked around every glyph in the atlas, and added to every quad, so
    // the fragment shader's outline ring has texels to read instead of clipping
    // at the glyph's own edge. The atlas padding must exceed the quad padding
    // plus the outline radius, or a fragment at the quad's edge would reach past
    // the empty gutter and smear a neighbouring glyph into the outline.
    constexpr int kAtlasPadding = 6;
    constexpr float kQuadPadding = 3.0f;

    // Same asset-root fallbacks Shader/PostFX use: the editor runs from the repo
    // root, an installed game runs from beside its own exe, and the web build
    // reads the preloaded emscripten FS.
    std::vector<unsigned char> readBinaryAsset(const std::string &path)
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> candidates;
        candidates.emplace_back(path);
#ifdef ASSET_ROOT
        candidates.emplace_back(fs::path(ASSET_ROOT) / path);
#endif
        candidates.emplace_back(fs::path("./") / path);
        candidates.emplace_back(fs::path("../") / path);

        for (const auto &c : candidates)
        {
            std::ifstream f(c, std::ios::binary | std::ios::ate);
            if (!f)
                continue;
            std::streamsize size = f.tellg();
            if (size <= 0)
                continue;
            f.seekg(0, std::ios::beg);
            std::vector<unsigned char> bytes(static_cast<size_t>(size));
            if (f.read(reinterpret_cast<char *>(bytes.data()), size))
                return bytes;
        }
        return {};
    }
}

TextRenderer::~TextRenderer()
{
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    if (atlas_)
        glDeleteTextures(1, &atlas_);
    delete shader_;
}

bool TextRenderer::init(const std::string &fontPath, float atlasPixelHeight)
{
    if (ready_)
        return true;

    std::vector<unsigned char> font = readBinaryAsset(fontPath);
    if (font.empty())
    {
        std::cerr << "TextRenderer: font not found: " << fontPath
                  << " — text rendering disabled\n";
        return false;
    }

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, font.data(), stbtt_GetFontOffsetForIndex(font.data(), 0)))
    {
        std::cerr << "TextRenderer: '" << fontPath << "' is not a usable TrueType face\n";
        return false;
    }

    // Pack the printable-ASCII range, growing the atlas until it all fits. The
    // starting size holds a 64px face comfortably; the loop is what keeps a
    // larger atlasPixelHeight from silently dropping glyphs.
    std::vector<stbtt_packedchar> packed(kCharCount);
    std::vector<unsigned char> bitmap;
    int size = 512;
    bool packedOk = false;
    for (; size <= 2048 && !packedOk; size *= 2)
    {
        bitmap.assign(static_cast<size_t>(size) * size, 0);
        stbtt_pack_context ctx;
        if (!stbtt_PackBegin(&ctx, bitmap.data(), size, size, 0, kAtlasPadding, nullptr))
            continue;
        packedOk = stbtt_PackFontRange(&ctx, font.data(), 0, atlasPixelHeight,
                                       kFirstChar, kCharCount, packed.data()) != 0;
        stbtt_PackEnd(&ctx);
    }
    if (!packedOk)
    {
        std::cerr << "TextRenderer: could not pack a glyph atlas at "
                  << atlasPixelHeight << "px\n";
        return false;
    }
    size /= 2; // the loop increments past the size that succeeded
    atlasSize_ = size;

    // Convert each packed glyph into em units: x/y in multiples of the label
    // height, y up with the baseline at 0, so a draw only has to scale.
    const float invPx = 1.0f / atlasPixelHeight;
    const float uvPad = kQuadPadding / static_cast<float>(size);
    for (int i = 0; i < kCharCount; ++i)
    {
        float x = 0.0f, y = 0.0f;
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(packed.data(), size, size, i, &x, &y, &q, 0);

        Glyph &g = glyphs_[i];
        g.advance = packed[i].xadvance * invPx;
        // Whitespace has no bitmap — it only advances the pen.
        g.baked = (q.x1 > q.x0) && (q.y1 > q.y0);
        if (!g.baked)
            continue;

        g.x0 = (q.x0 - kQuadPadding) * invPx;
        g.x1 = (q.x1 + kQuadPadding) * invPx;
        // stb's quad is y-down from the baseline; negate to get y-up em space.
        g.y0 = -(q.y1 + kQuadPadding) * invPx;
        g.y1 = -(q.y0 - kQuadPadding) * invPx;
        g.u0 = q.s0 - uvPad;
        g.u1 = q.s1 + uvPad;
        g.v0 = q.t0 - uvPad;
        g.v1 = q.t1 + uvPad;
    }

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    ascent_ = ascent * stbtt_ScaleForPixelHeight(&info, atlasPixelHeight) * invPx;

    shader_ = new Shader("./shaders/text_vert.glsl", "./shaders/text_frag.glsl");
    // Shader falls back to a built-in wireframe program when its files are
    // missing rather than failing; the text uniforms are how we notice.
    if (glGetUniformLocation(shader_->ID, "uAtlas") < 0 ||
        glGetUniformLocation(shader_->ID, "uViewProj") < 0)
    {
        std::cerr << "TextRenderer: text shaders failed to load — text rendering disabled\n";
        delete shader_;
        shader_ = nullptr;
        return false;
    }

    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    ready_ = true;
    return true;
}

float TextRenderer::measure(const std::string &text) const
{
    if (!ready_)
        return 0.0f;
    float w = 0.0f;
    for (unsigned char c : text)
    {
        if (c < kFirstChar || c >= kFirstChar + kCharCount)
            continue;
        w += glyphs_[c - kFirstChar].advance;
    }
    return w;
}

float TextRenderer::buildQuads(const std::string &text)
{
    float pen = 0.0f;
    for (unsigned char c : text)
    {
        if (c < kFirstChar || c >= kFirstChar + kCharCount)
            continue; // not in the atlas: drop it rather than draw a hole
        const Glyph &g = glyphs_[c - kFirstChar];
        if (g.baked)
        {
            const float x0 = pen + g.x0, x1 = pen + g.x1;
            // Two triangles, counter-clockwise. v grows downward in the atlas,
            // so the top edge of the quad samples v0.
            const float quad[6][4] = {
                {x0, g.y0, g.u0, g.v1},
                {x1, g.y0, g.u1, g.v1},
                {x1, g.y1, g.u1, g.v0},
                {x0, g.y0, g.u0, g.v1},
                {x1, g.y1, g.u1, g.v0},
                {x0, g.y1, g.u0, g.v0},
            };
            verts_.insert(verts_.end(), &quad[0][0], &quad[0][0] + 24);
        }
        pen += g.advance;
    }
    return pen;
}

void TextRenderer::submit(const glm::mat4 &viewProj, const glm::vec3 &origin,
                          const glm::vec3 &right, const glm::vec3 &up,
                          const glm::vec4 &color, bool depthTested)
{
    shader_->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    shader_->setInt("uAtlas", 0);
    shader_->setMat4("uViewProj", viewProj);
    shader_->setVec3("uOrigin", origin);
    shader_->setVec3("uRight", right);
    shader_->setVec3("uUp", up);
    shader_->setVec4("uColor", color);
    shader_->setVec4("uOutlineColor", outlineColor_);
    shader_->setVec2("uTexel", glm::vec2(1.0f / static_cast<float>(atlasSize_)));
    shader_->setFloat("uOutline", outlineRadius_);

#if !defined(__EMSCRIPTEN__)
    // renderSystem leaves the polygon mode on GL_LINE; glyph quads must fill.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // World labels keep the caller's depth test, so geometry in front of them
    // occludes them. HUD text must not: it is drawn after the scene has been
    // composited to a framebuffer whose depth buffer holds nothing meaningful,
    // and testing against it swallows the text entirely.
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (!depthTested)
        glDisable(GL_DEPTH_TEST);
    // Never write depth either way — overlapping labels must not punch holes in
    // each other.
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const size_t bytes = verts_.size() * sizeof(float);
    if (bytes > vboCapacity_)
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, verts_.data(), GL_DYNAMIC_DRAW);
        vboCapacity_ = bytes;
    }
    else
    {
        // Orphan the old storage so the driver doesn't stall waiting on the
        // previous label's draw before overwriting it.
        glBufferData(GL_ARRAY_BUFFER, vboCapacity_, nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, verts_.data());
    }
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts_.size() / 4));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    if (!depthTested && depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void TextRenderer::drawBillboard(const std::string &text, const glm::vec3 &worldPos,
                                 float worldHeight, const glm::vec4 &color,
                                 const glm::mat4 &view, const glm::mat4 &projection)
{
    if (!ready_ || text.empty() || worldHeight <= 0.0f)
        return;

    verts_.clear();
    const float width = buildQuads(text);
    if (verts_.empty())
        return;

    // The view matrix's first two columns-of-rows are the camera's right and up
    // axes in world space; using them as the quad basis is what faces the label
    // at the camera without a per-label matrix.
    const glm::vec3 right(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 up(view[0][1], view[1][1], view[2][1]);
    const glm::vec3 origin = worldPos - right * (width * 0.5f * worldHeight);

    submit(projection * view, origin, right * worldHeight, up * worldHeight, color, true);
}

void TextRenderer::drawScreen(const std::string &text, float x, float y,
                              float pixelHeight, const glm::vec4 &color,
                              int fbWidth, int fbHeight)
{
    if (!ready_ || text.empty() || pixelHeight <= 0.0f || fbWidth < 1 || fbHeight < 1)
        return;

    verts_.clear();
    buildQuads(text);
    if (verts_.empty())
        return;

    // Pixel space with the origin top-left, so +y in em space points at -y here.
    const glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(fbWidth),
                                       static_cast<float>(fbHeight), 0.0f, -1.0f, 1.0f);
    const glm::vec3 origin(x, y + ascent_ * pixelHeight, 0.0f);
    submit(ortho, origin,
           glm::vec3(pixelHeight, 0.0f, 0.0f),
           glm::vec3(0.0f, -pixelHeight, 0.0f), color, false);
}
