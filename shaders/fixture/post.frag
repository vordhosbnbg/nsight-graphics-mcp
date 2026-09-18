#version 450

layout(set = 0, binding = 0) uniform sampler2D scene_color;
layout(push_constant) uniform PostSettings {
    uint channel_order;
} settings;
layout(location = 0) out vec4 output_color;

void main() {
    vec3 input_color = texelFetch(scene_color, ivec2(gl_FragCoord.xy), 0).rgb;
    if(settings.channel_order != 0u) {
        input_color = input_color.bgr;
    }
    output_color = vec4(input_color * vec3(0.75, 0.875, 0.5) + vec3(0.03125, 0.015625, 0.0625), 1.0);
}
