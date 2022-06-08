#version 450

// G-buffer
layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec4 out_albedo_specular;

layout (location = 0) in vec2 in_tex_coord;
layout (location = 1) in vec3 in_position;
layout (location = 2) in vec3 in_normal;


layout(set = 2, binding = 0) uniform sampler2D tex;

void main()
{
    // Position
    out_position = in_position;
    // Normal
    out_normal = in_normal;
    // Albedo
    out_albedo_specular.rgb = texture(tex, in_tex_coord).rgb;
    // Specular
    out_albedo_specular.a = 1.0;
}
