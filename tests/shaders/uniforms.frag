#version 460
layout(location = 0) out vec4 color;
layout(set = 2, binding = 5, std140) uniform Inputs {
    vec4 floating;
    ivec2 signed_values;
    uvec2 unsigned_values;
} inputs;
void main() {
    color = inputs.floating + vec4(vec2(inputs.signed_values), vec2(inputs.unsigned_values));
}
