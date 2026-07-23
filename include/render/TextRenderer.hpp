#ifndef RENDER_TEXT_RENDERER_HPP
#define RENDER_TEXT_RENDERER_HPP

#include <glm/glm.hpp>

#include <string>
#include <vector>

class Shader;

// Engine text rendering. A TrueType face is rasterised once at startup into a
// single-channel glyph atlas (stb_truetype), and each draw builds one quad per
// glyph into a shared dynamic buffer — a label costs one draw call.
//
// Two entry points share the atlas and the shader:
//   * drawBillboard — a world-space label that always faces the camera. This is
//     what nametags use (see ecs::nametagSystem).
//   * drawScreen    — a pixel-space label for HUD text.
//
// Only printable ASCII (0x20..0x7E) is baked; other bytes are skipped, so the
// only text that ever reaches the GPU is text the atlas can actually draw.
// Callers that accept untrusted strings (player names off the wire) sanitise
// them to that range first.
//
// If the font or the shader can't be loaded the renderer stays disabled and
// every draw is a no-op, so a stripped install degrades quietly rather than
// crashing. Requires a live GL context; never constructed in headless builds.
class TextRenderer
{
public:
    TextRenderer() = default;
    ~TextRenderer();
    TextRenderer(const TextRenderer &) = delete;
    TextRenderer &operator=(const TextRenderer &) = delete;

    // Rasterise `fontPath` (a .ttf, resolved against the usual asset roots) and
    // compile the text shader. `atlasPixelHeight` is the rasterisation size, not
    // the display size — draws scale the glyphs freely, so this only sets how
    // much detail the atlas holds.
    bool init(const std::string &fontPath, float atlasPixelHeight = 64.0f);
    bool ready() const { return ready_; }

    // Width of `text` when drawn at height 1.0. Multiply by the height passed to
    // a draw call to get the width in that call's units.
    float measure(const std::string &text) const;

    // Camera-facing label, horizontally centred on `worldPos` with its baseline
    // there. `worldHeight` is the em height in world units.
    void drawBillboard(const std::string &text, const glm::vec3 &worldPos,
                       float worldHeight, const glm::vec4 &color,
                       const glm::mat4 &view, const glm::mat4 &projection);

    // Pixel-space label with its top-left corner at (x, y), y growing downward,
    // in a framebuffer of fbWidth x fbHeight pixels.
    void drawScreen(const std::string &text, float x, float y, float pixelHeight,
                    const glm::vec4 &color, int fbWidth, int fbHeight);

    // Colour drawn behind the glyph edges (see the outline in text_frag.glsl).
    void setOutline(const glm::vec4 &color, float radiusTexels)
    {
        outlineColor_ = color;
        outlineRadius_ = radiusTexels;
    }

private:
    // One glyph quad in em units (y up, baseline at 0) plus its atlas rect.
    struct Glyph
    {
        float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
        float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
        float advance = 0.f;
        bool baked = false;
    };

    // Append `text`'s quads to verts_, laid out from the origin along +x.
    // Returns the total advance width (in em units).
    float buildQuads(const std::string &text);
    // Issue verts_ with the given placement basis.
    void submit(const glm::mat4 &viewProj, const glm::vec3 &origin,
                const glm::vec3 &right, const glm::vec3 &up, const glm::vec4 &color,
                bool depthTested);

    static constexpr int kFirstChar = 32;  // ' '
    static constexpr int kCharCount = 95;  // through '~'

    bool ready_ = false;
    Shader *shader_ = nullptr;
    unsigned int atlas_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    size_t vboCapacity_ = 0; // bytes currently allocated in vbo_

    int atlasSize_ = 0;
    float ascent_ = 0.8f; // baseline offset below the top of the line, in em units
    Glyph glyphs_[kCharCount];

    glm::vec4 outlineColor_{0.0f, 0.0f, 0.0f, 1.0f};
    float outlineRadius_ = 1.6f;

    std::vector<float> verts_; // interleaved: pos.xy, uv.xy
};

#endif // RENDER_TEXT_RENDERER_HPP
