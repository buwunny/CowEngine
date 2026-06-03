#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity;
uniform int uTonemap;       // 1 = apply Reinhard + gamma (only meaningful when bloom is on)
uniform int uScanlines;
uniform float uScanlineStrength;
uniform float uTime;
uniform vec2 uOutputSize;

vec3 tonemap(vec3 x)
{
    return x / (x + vec3(1.0));
}

void main()
{
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 col = scene + bloom * uBloomIntensity;

    if (uTonemap == 1)
    {
        col = tonemap(col);
        col = pow(col, vec3(1.0 / 2.2));
    }

    if (uScanlines == 1)
    {
        float line = sin(vUV.y * uOutputSize.y * 3.14159);
        float dark = mix(1.0, 0.5 + 0.5 * line, uScanlineStrength);
        col *= dark;
    }

    FragColor = vec4(col, 1.0);
}
