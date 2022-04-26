//glsl version 4.5
#version 450

//output write
layout (location = 0) in vec3 inColor;
layout (location = 0) out vec4 outFragColor;

void main()
{
    //return red
    outFragColor = vec4(inColor, 1.0f);
}
