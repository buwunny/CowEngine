#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec2 uTexelStep;   // (1/w, 0) for horizontal pass, (0, 1/h) for vertical

void main()
{
    // 9-tap Gaussian (sigma ~ 2.0). Symmetric around the center sample.
    const float w0 = 0.227027;
    const float w1 = 0.194595;
    const float w2 = 0.121622;
    const float w3 = 0.054054;
    const float w4 = 0.016216;

    vec3 sum = texture(uTex, vUV).rgb * w0;
    sum += texture(uTex, vUV + uTexelStep * 1.0).rgb * w1;
    sum += texture(uTex, vUV - uTexelStep * 1.0).rgb * w1;
    sum += texture(uTex, vUV + uTexelStep * 2.0).rgb * w2;
    sum += texture(uTex, vUV - uTexelStep * 2.0).rgb * w2;
    sum += texture(uTex, vUV + uTexelStep * 3.0).rgb * w3;
    sum += texture(uTex, vUV - uTexelStep * 3.0).rgb * w3;
    sum += texture(uTex, vUV + uTexelStep * 4.0).rgb * w4;
    sum += texture(uTex, vUV - uTexelStep * 4.0).rgb * w4;
    FragColor = vec4(sum, 1.0);
}
