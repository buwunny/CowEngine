#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform float uThreshold;
uniform float uSoftKnee;

void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    float lum = max(max(c.r, c.g), c.b);
    // Soft knee around threshold so bright lines bleed smoothly into bloom
    float knee = max(uSoftKnee, 1e-4);
    float soft = clamp((lum - uThreshold + knee) / (2.0 * knee), 0.0, 1.0);
    float contrib = max(lum - uThreshold, 0.0);
    contrib = mix(contrib, soft * soft * knee, soft);
    float scale = (lum > 1e-4) ? (contrib / lum) : 0.0;
    FragColor = vec4(c * scale, 1.0);
}
