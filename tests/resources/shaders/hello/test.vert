//we will be using glsl version 4.5 syntax
#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} camera;

layout (location = 0) out vec3 outColor;

void main() {
    //output the position of each vertex
    gl_Position = camera.view_projection * vec4(position, 1.0f);
    outColor = color;
}
