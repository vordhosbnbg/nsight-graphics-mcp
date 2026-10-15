#version 460
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0, std140) uniform Palette { vec4 tint; } palette;
layout(set = 0, binding = 1, std430) buffer Counter { uint value; } counter;
void main() {
    atomicAdd(counter.value, 1u);
    color = palette.tint;
}
