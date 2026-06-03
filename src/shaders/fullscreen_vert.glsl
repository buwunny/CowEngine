#version 330 core
// Big-triangle fullscreen vertex shader. Driven entirely by gl_VertexID, so
// the caller only needs to bind any non-zero VAO and glDrawArrays(GL_TRIANGLES, 0, 3).
out vec2 vUV;
void main()
{
    vec2 pos = vec2(
        (gl_VertexID == 1) ? 3.0 : -1.0,
        (gl_VertexID == 2) ? 3.0 : -1.0);
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
