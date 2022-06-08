#version 450

layout (location = 0) out vec2 out_tex_coords;

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

// Texture coordinates
const vec2 tex_coords[6] = vec2[6](
    // Triangle 1
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    // Triangle 2
    vec2(1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0)
);

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
    out_tex_coords = tex_coords[gl_VertexIndex];
}