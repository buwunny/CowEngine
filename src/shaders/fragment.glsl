#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec4 wireframeColor;
uniform vec3 uCamPos;

uniform int uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

uniform float uNeonIntensity;

void main()
{
    vec3 base = wireframeColor.rgb * uNeonIntensity;
    if (uFogEnabled == 1)
    {
        float dist = distance(vWorldPos, uCamPos);
        float f = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 1e-3), 0.0, 1.0);
        // Fade toward fog color but keep some neon to still register in bloom.
        base = mix(base, uFogColor, f * 0.85);
    }
    FragColor = vec4(base, wireframeColor.a);
}
