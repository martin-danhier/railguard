#version 450

// 2 triangles over the screen
const vec2 vertices[6] = vec2[6](
    // Triangle 1
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    // Triangle 2
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0)
);

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
}