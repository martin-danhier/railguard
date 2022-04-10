//we will be using glsl version 4.5 syntax
#version 450

layout(set = 0, binding = 0) readonly uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} camera;

layout (location = 0) out vec3 outColor;

void main()
{
    // Triangle data
    const vec3 positions[8] = vec3[8](
    vec3(-1.0, -1.0, 0.0),
    vec3(1.0, -1.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(-1.0, -1.0, 0.0),
    vec3(1.0, -1.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(-1.0, -1.0, 0.0),
    vec3(1.0, -1.0, 0.0)
    );
    const vec3 colors[8] = vec3[8](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(1.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0),
    vec3(0.5, 0.5, 0.5)
    );

    //output the position of each vertex


    gl_Position =  vec4(positions[gl_VertexIndex], 1.0f);
    outColor = colors[gl_VertexIndex];
}
