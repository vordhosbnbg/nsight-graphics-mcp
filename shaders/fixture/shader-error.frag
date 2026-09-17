#version 450

layout(set = 0, binding = 0, std140) uniform Palette {
    vec4 tint;
} palette;
layout(location = 0) in vec3 vertex_color;
layout(location = 0) out vec4 output_color;

void main() {
    output_color = vec4(vertex_color.bgr * palette.tint.rgb, 1.0);
}
