#version 450

layout (location = 0) in vec2 in_tex_coords;
layout (location = 0) out vec4 out_frag_color;

// G-Buffer input
layout (set = 2, binding = 0) uniform sampler2D in_position;
layout (set = 2, binding = 1) uniform sampler2D in_normal;
layout (set = 2, binding = 2) uniform sampler2D in_albedo_specular;

void main() {
    // Get G-Buffer data related to this fragment
    vec3 position  = texture(in_position, in_tex_coords).rgb;
    vec3 normal    = texture(in_normal, in_tex_coords).rgb;
    vec3 albedo    = texture(in_albedo_specular, in_tex_coords).rgb;
    float specular = texture(in_albedo_specular, in_tex_coords).a;

    // Compute final color
    out_frag_color = vec4(albedo, 1.0);
}