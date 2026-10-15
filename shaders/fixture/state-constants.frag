#version 450

layout(set = 0, binding = 0, std140) uniform Palette {
    vec4 tint;
} palette;
layout(push_constant, std430) uniform Selection {
    uint red;
    uint green;
    uint blue;
    uint retained;
} selection;
layout(location = 0) in vec3 vertex_color;
layout(location = 0) out vec4 output_color;

void main() {
    output_color = vec4(vertex_color * palette.tint.rgb *
                        vec3(selection.red, selection.green, selection.blue), 1.0);
}
