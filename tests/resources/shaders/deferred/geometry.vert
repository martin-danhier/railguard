#version 450

// Vertex input
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;

// Camera data
layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} camera;

// Global data
struct ObjectData {
    mat4 transform;
};
layout(set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objectBuffer;

// Output
layout (location = 0) out vec2 out_tex_coords;
layout (location = 1) out vec3 out_position;
layout (location = 2) out vec3 out_normal;

void main() {
    //output the position of each vertex
    ObjectData current_object = objectBuffer.objects[0];
    gl_Position = camera.view_projection * current_object.transform * vec4(position, 1.0f);

    // Transmit info to fragment shader
    out_tex_coords = tex_coords;
    out_position = position;
    out_normal = normal;
}
