#version 450

layout (location = 0) out vec3 out_color;

// Define triangle vertices for equilateral triangle
const vec3 vertices[3] = vec3[3](
    vec3(-0.5, -0.5, 0.0),
    vec3(0.5, -0.5, 0.0),
    vec3(0.0, 0.5, 0.0)
);

// Define colors for each vertex
const vec3 colors[3] = vec3[3](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    out_color = colors[gl_VertexIndex];
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0);
}