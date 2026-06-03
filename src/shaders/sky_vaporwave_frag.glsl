#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCamPos;

uniform vec3 uSkyTop;
uniform vec3 uSkyMid;
uniform vec3 uSkyBottom;

uniform vec2 uSunPos;       // world-anchored mode: NDC (x, y in [-1,1])
                            // screen-anchored mode: (x in [-1,1], height above horizon)
uniform int uSunWorldAnchored;
uniform int uSunVisible;
uniform float uSunRadius;
uniform vec3 uSunColor;
uniform int uSunStripes;
uniform int uSkyEnabled;

uniform float uAspect;      // width / height — keeps the sun disk circular

uniform int uGridEnabled;
uniform vec3 uGridColor;
uniform float uGridScale;
uniform float uGridFade;
uniform float uGridLineWidth;
uniform float uGridPlaneY;

float gridLine(float v, float w)
{
    // Distance from nearest integer line; smoothstep returns 1 on the line, 0 between.
    float d = abs(fract(v - 0.5) - 0.5);
    float aa = fwidth(v) * 1.5;
    return 1.0 - smoothstep(w - aa, w + aa, d);
}

void main()
{
    vec2 ndc = vUV * 2.0 - 1.0;

    // Reconstruct a world-space ray for this pixel
    vec4 nearH = uInvViewProj * vec4(ndc, -1.0, 1.0);
    vec4 farH  = uInvViewProj * vec4(ndc,  1.0, 1.0);
    vec3 nearW = nearH.xyz / nearH.w;
    vec3 farW  = farH.xyz  / farH.w;
    vec3 rayDir = normalize(farW - nearW);

    // Gradient sky uses screen-y so the bands stay fixed on screen.
    float t = clamp(vUV.y, 0.0, 1.0);
    vec3 lower = mix(uSkyBottom, uSkyMid, smoothstep(0.0, 0.55, t));
    vec3 sky   = mix(lower,      uSkyTop, smoothstep(0.45, 1.0, t));

    if (uSkyEnabled == 0)
        sky = vec3(0.02, 0.02, 0.04);

    // Glowing striped sun. World-anchored mode receives the sun's projected
    // NDC position; screen-anchored mode uses sunPos.y as "height above the
    // (vUV.y = 0.5) horizon line", scaled to half-screen.
    vec2 sunCenter;
    if (uSunWorldAnchored == 1)
        sunCenter = uSunPos * 0.5 + 0.5;
    else
        sunCenter = vec2(uSunPos.x * 0.5 + 0.5, 0.5 + uSunPos.y * 0.5);
    vec2 sunUV = vUV - sunCenter;
    // Multiply x by aspect (width/height) so equal sunUV distances map to
    // equal pixel distances — without this, the disk stretches horizontally
    // on a wide viewport.
    sunUV.x *= uAspect;
    float sunDist = length(sunUV);
    float sunDisk = smoothstep(uSunRadius, uSunRadius * 0.85, sunDist);
    float sunGlow = exp(-pow(sunDist / max(uSunRadius, 1e-4), 1.6) * 2.0);

    // Horizontal stripes cut out the lower half of the disk
    if (uSunStripes > 0)
    {
        float relY = (sunUV.y / max(uSunRadius, 1e-4)) * 0.5 + 0.5;  // 0..1 within disk
        // Stripes grow thicker toward the bottom of the sun
        float bandT = 1.0 - relY;
        float stripeMask = step(0.0, sin(bandT * 3.14159 * float(uSunStripes) * (0.5 + bandT)));
        // Only cut stripes in lower half of the sun
        float lowerHalf = smoothstep(0.55, 0.45, relY);
        sunDisk *= mix(1.0, stripeMask, lowerHalf);
    }

    vec3 col = sky;
    if (uSunVisible == 1)
    {
        col += uSunColor * sunDisk;
        col += uSunColor * sunGlow * 0.45;
    }

    // Perspective grid floor — intersect ray with y = uGridPlaneY
    if (uGridEnabled == 1)
    {
        float denom = rayDir.y;
        // Only draw where the ray points down toward the plane below the camera
        if (abs(denom) > 1e-4)
        {
            float tHit = (uGridPlaneY - uCamPos.y) / denom;
            if (tHit > 0.0)
            {
                vec3 hit = uCamPos + rayDir * tHit;
                vec2 g = hit.xz / max(uGridScale, 1e-3);
                float lx = gridLine(g.x, uGridLineWidth);
                float lz = gridLine(g.y, uGridLineWidth);
                float lineMask = max(lx, lz);

                // Distance fade
                float dist = length(hit.xz - uCamPos.xz);
                float fade = 1.0 - smoothstep(uGridFade * 0.2, uGridFade, dist);
                // Horizon haze — soften lines near the vanishing line
                float horizonSoft = smoothstep(0.0, 0.15, abs(denom));

                vec3 gridCol = uGridColor * lineMask * fade * horizonSoft;
                // Boost intensity so bloom catches the brightest lines
                col += gridCol * 1.6;
            }
        }
    }

    FragColor = vec4(col, 1.0);
}
