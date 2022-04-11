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
    vec3(1.0f, 1.0f, -1.0f), // triangle 2 : begin
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, 1.0f, -1.0f), // triangle 2 : end
    vec3(1.0f, -1.0f, 1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(1.0f, 1.0f, -1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, -1.0f),
    vec3(1.0f, -1.0f, 1.0f),
    vec3(-1.0f, -1.0f, 1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(-1.0f, -1.0f, 1.0f),
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
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(1.0f, -1.0f, 1.0f)
    );
    const vec3 colors[36] = vec3[36](
    vec3(0.583f, 0.771f, 0.014f),
    vec3(0.583f, 0.771f, 0.014f),
    vec3(0.583f, 0.771f, 0.014f),
    vec3(0.583f, 0.771f, 0.014f),
    vec3(0.609f, 0.115f, 0.436f),
    vec3(0.327f, 0.483f, 0.844f),

    vec3(0.822f, 0.569f, 0.201f),
    vec3(0.435f, 0.602f, 0.223f),
    vec3(0.310f, 0.747f, 0.185f),
    vec3(0.597f, 0.770f, 0.761f),
    vec3(0.559f, 0.436f, 0.730f),
    vec3(0.359f, 0.583f, 0.152f),

    vec3(0.014f, 0.184f, 0.576f),
    vec3(0.771f, 0.328f, 0.970f),
    vec3(0.406f, 0.615f, 0.116f),
    vec3(0.676f, 0.977f, 0.133f),
    vec3(0.971f, 0.572f, 0.833f),
    vec3(0.140f, 0.616f, 0.489f),

    vec3(0.997f, 0.513f, 0.064f),
    vec3(0.945f, 0.719f, 0.592f),
    vec3(0.279f, 0.317f, 0.505f),
    vec3(0.167f, 0.620f, 0.077f),
    vec3(0.347f, 0.857f, 0.137f),
    vec3(0.055f, 0.953f, 0.042f),

    vec3(0.714f, 0.505f, 0.345f),
    vec3(0.783f, 0.290f, 0.734f),
    vec3(0.302f, 0.455f, 0.848f),
    vec3(0.167f, 0.620f, 0.077f),
    vec3(0.225f, 0.587f, 0.040f),
    vec3(0.783f, 0.290f, 0.734f),

    vec3(0.783f, 0.290f, 0.734f),
    vec3(0.543f, 0.021f, 0.978f),
    vec3(0.914f, 0.042f, 0.042f),
    vec3(0.135f, 0.609f, 0.318f),
    vec3(0.188f, 0.576f, 0.024f),
    vec3(0.914f, 0.042f, 0.042f)
    );

    //output the position of each vertex


    gl_Position = camera.view_projection * vec4(positions[gl_VertexIndex], 1.0f);
    outColor = colors[gl_VertexIndex];
}
