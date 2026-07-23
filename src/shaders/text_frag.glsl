#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uAtlas;
uniform vec4 uColor;
uniform vec4 uOutlineColor;
uniform vec2 uTexel;    // 1 / atlas size
uniform float uOutline; // outline radius in texels; 0 disables it

void main()
{
    // The atlas is single-channel coverage produced by stb_truetype.
    float glyph = texture(uAtlas, vUV).r;

    // Dilate the coverage by sampling a ring around the texel: the difference
    // between the dilated and original coverage is the outline. Without it,
    // light text over the neon wireframe scene reads as noise wherever a bright
    // object passes behind it.
    float solid = glyph;
    if (uOutline > 0.0)
    {
        vec2 d = uTexel * uOutline;
        solid = max(solid, texture(uAtlas, vUV + vec2( d.x,  0.0)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2(-d.x,  0.0)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2( 0.0,  d.y)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2( 0.0, -d.y)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2( d.x,  d.y)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2(-d.x,  d.y)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2( d.x, -d.y)).r);
        solid = max(solid, texture(uAtlas, vUV + vec2(-d.x, -d.y)).r);
    }

    if (solid <= 0.0)
        discard;

    vec3 rgb = mix(uOutlineColor.rgb, uColor.rgb, glyph);
    FragColor = vec4(rgb, solid * uColor.a);
}
