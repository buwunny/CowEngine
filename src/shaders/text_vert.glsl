#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

// One shader serves both text modes. Glyph quads arrive in flat "text units"
// (x right, y up, 1.0 = the label's height) and are placed by a basis the
// caller supplies: for a world billboard uRight/uUp are the camera's axes
// scaled to the label height, for screen text they are the pixel axes and
// uViewProj is an orthographic matrix.
uniform mat4 uViewProj;
uniform vec3 uOrigin;
uniform vec3 uRight;
uniform vec3 uUp;

out vec2 vUV;

void main()
{
    vec3 world = uOrigin + uRight * aPos.x + uUp * aPos.y;
    vUV = aUV;
    gl_Position = uViewProj * vec4(world, 1.0);
}
