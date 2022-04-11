//we will be using glsl version 4.5 syntax
#version 450

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} camera;

layout (location = 0) out vec3 outColor;

void main()
{
    // Cube data
    // Three vertices = one triangle
    const vec3 positions[36] = vec3[36](
    vec3(-1.0f, -1.0f, -1.0f), // triangle 1 : begin
    vec3(-1.0f, -1.0f, 1.0f),
    vec3(-1.0f, 1.0f, 1.0f), // triangle 1 : end
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, -1.0f),

    vec3(1.0f, 1.0f, -1.0f), // triangle 2 : begin
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, 1.0f, -1.0f), // triangle 2 : end
    vec3(1.0f, 1.0f, -1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(-1.0f, -1.0f, -1.0f),

    vec3(1.0f, -1.0f, 1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(1.0f, -1.0f, 1.0f),
    vec3(-1.0f, -1.0f, 1.0f),
    vec3(-1.0f, -1.0f, -1.0f),

    vec3(-1.0f, 1.0f, 1.0f),
    vec3(-1.0f, -1.0f, 1.0f),
    vec3(1.0f, -1.0f, 1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(1.0f, -1.0f, 1.0f),

    vec3(1.0f, 1.0f, 1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(1.0f, 1.0f, -1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(1.0f, -1.0f, 1.0f),

    vec3(1.0f, 1.0f, 1.0f),
    vec3(1.0f, 1.0f, -1.0f),
    vec3(-1.0f, 1.0f, -1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, -1.0f),
    vec3(-1.0f, 1.0f, 1.0f)
    );
    const vec3 colors[36] = vec3[36](
    // Face 1 in red
    vec3(1.0f, 0.0f, 0.0f),
    vec3(1.0f, 0.0f, 0.0f),
    vec3(1.0f, 0.0f, 0.0f),
    vec3(1.0f, 0.0f, 0.0f),
    vec3(1.0f, 0.0f, 0.0f),
    vec3(1.0f, 0.0f, 0.0f),
    // Face 2 in green
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    // Face 3 in blue
    vec3(0.0f, 0.0f, 1.0f),
    vec3(0.0f, 0.0f, 1.0f),
    vec3(0.0f, 0.0f, 1.0f),
    vec3(0.0f, 0.0f, 1.0f),
    vec3(0.0f, 0.0f, 1.0f),
    vec3(0.0f, 0.0f, 1.0f),
    // Face 4 in yellow
    vec3(1.0f, 1.0f, 0.0f),
    vec3(1.0f, 1.0f, 0.0f),
    vec3(1.0f, 1.0f, 0.0f),
    vec3(1.0f, 1.0f, 0.0f),
    vec3(1.0f, 1.0f, 0.0f),
    vec3(1.0f, 1.0f, 0.0f),
    // Face 5 in magenta
    vec3(1.0f, 0.0f, 1.0f),
    vec3(1.0f, 0.0f, 1.0f),
    vec3(1.0f, 0.0f, 1.0f),
    vec3(1.0f, 0.0f, 1.0f),
    vec3(1.0f, 0.0f, 1.0f),
    vec3(1.0f, 0.0f, 1.0f),
    // Face 5 in cyan
    vec3(0.0f, 1.0f, 1.0f),
    vec3(0.0f, 1.0f, 1.0f),
    vec3(0.0f, 1.0f, 1.0f),
    vec3(0.0f, 1.0f, 1.0f),
    vec3(0.0f, 1.0f, 1.0f),
    vec3(0.0f, 1.0f, 1.0f)


    );

    //output the position of each vertex


    gl_Position = camera.view_projection * vec4(positions[gl_VertexIndex], 1.0f);
    outColor = colors[gl_VertexIndex];
}
