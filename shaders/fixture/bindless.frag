#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0, std430) readonly buffer Palette {
    vec4 tint;
} palettes[];
layout(push_constant) uniform Selection {
    uint resource_xor;
    uint phase;
} selection;
layout(location = 0) in vec3 vertex_color;
layout(location = 0) out vec4 output_color;

void main() {
    uint resource_index = ((uint(gl_FragCoord.x) / 8u) ^ selection.phase ^ selection.resource_xor) & 1u;
    output_color = vec4(vertex_color * palettes[nonuniformEXT(resource_index)].tint.rgb, 1.0);
}
