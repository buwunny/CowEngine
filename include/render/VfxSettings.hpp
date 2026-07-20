#ifndef VFX_SETTINGS_HPP
#define VFX_SETTINGS_HPP

#include <glm/glm.hpp>

// Vaporwave / VFX settings. Read by Application each frame to drive the sky-grid
// background pass, the main shader's fog + neon intensity, and the bloom
// post-process chain.
//
// This is a plain-data struct with no ImGui dependency on purpose: the runtime
// (PostFX, Application's standalone game loop) needs it, so it must be
// includable by targets that do not link the editor UI. The editor's Context
// aliases it as editor::Context::VFX.
namespace editor
{
    struct VFX
    {
        // Every visual feature is gated by its own *Enabled flag so the
        // editor boots into a plain wireframe-on-dark look. Users opt in
        // to each effect via the VFX panel.

        // Sky gradient
        bool skyEnabled = false;
        glm::vec3 skyTop = glm::vec3(0.05f, 0.02f, 0.18f);     // deep purple
        glm::vec3 skyMid = glm::vec3(0.85f, 0.18f, 0.55f);     // hot pink
        glm::vec3 skyBottom = glm::vec3(1.00f, 0.55f, 0.20f);  // sunset orange

        // Sun disk + glow
        bool sunEnabled = false;
        // sunPos is used only in screen-anchored mode (NDC: -1..1 on both axes).
        glm::vec2 sunPos = glm::vec2(0.0f, 0.18f);
        float sunRadius = 0.22f;
        glm::vec3 sunColor = glm::vec3(1.0f, 0.85f, 0.45f);
        int sunStripes = 6;
        // When true, the sun is fixed at a world-space direction (set by
        // azimuth + elevation) and projected to screen each frame so it
        // sits on the horizon rather than drifting with the camera.
        bool sunWorldAnchored = true;
        float sunAzimuth = 0.0f;    // degrees around Y, 0 = looking down -Z
        float sunElevation = 6.0f;  // degrees above the horizon

        // Perspective grid floor
        bool gridEnabled = false;
        glm::vec3 gridColor = glm::vec3(1.0f, 0.25f, 0.85f);   // neon magenta
        float gridScale = 4.0f;                                // world units between major lines
        float gridFade = 120.0f;                               // distance at which grid fades to 0
        float gridLineWidth = 0.04f;                           // line thickness (0..1)
        float horizonY = 0.0f;                                 // world-Y of the grid plane

        // Solid black fill behind each wireframe object (legibility — hides sky)
        bool wireframeFill = true;

        // Distance fog applied to the wireframe lines
        bool fogEnabled = false;
        glm::vec3 fogColor = glm::vec3(0.30f, 0.05f, 0.30f);
        float fogStart = 12.0f;
        float fogEnd = 140.0f;

        // Neon brightness boost on wireframe colors (1.0 = pass-through)
        bool neonEnabled = false;
        float neonIntensity = 1.0f;

        // Bloom post-process (also enables HDR tonemap + gamma so neon stays balanced)
        bool bloomEnabled = false;
        float bloomThreshold = 0.55f;
        float bloomIntensity = 1.4f;
        float bloomRadius = 1.0f;                              // blur kernel scale
        int bloomIterations = 5;                               // ping-pong passes per axis pair

        // Retro CRT overlay
        bool scanlinesEnabled = false;
        float scanlineStrength = 0.15f;
    };
}

#endif // VFX_SETTINGS_HPP
