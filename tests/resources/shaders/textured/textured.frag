//glsl version 4.5
#version 450

//output write
layout (location = 0) out vec4 out_frag_color;
layout (location = 0) in vec2 in_tex_coord;
layout(set = 2, binding = 0) uniform sampler2D tex;

void main()
{
    vec3 color = texture(tex, in_tex_coord).xyz;
    out_frag_color = vec4(color, 1.0f);
}
