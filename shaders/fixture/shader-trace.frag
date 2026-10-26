#version 450

layout(set = 0, binding = 0, std140) uniform Palette {
    vec4 tint;
} palette;
layout(location = 0) in vec3 vertex_color;
layout(location = 0) out vec4 output_color;

// Optional application-owned std430 ABI from ngm/ShaderTrace.hpp. This probe
// changes the shader and makes no claim about the original program's values.
struct TraceRecord {
    uvec4 invocation;
    uvec4 selector;
    uvec4 bits;
};
layout(set = 0, binding = 1, std430) buffer Trace {
    uint capacity;
    uint attempted_records;
    uint dropped_records;
    uint pixel_x;
    uint pixel_y;
    uint frame;
    uint draw;
    uint reserved;
    TraceRecord records[];
} trace;

void observe(uint probe, uvec4 bits, uint components) {
    if (gl_HelperInvocation || uvec2(gl_FragCoord.xy) != uvec2(trace.pixel_x, trace.pixel_y))
        return;
    uint index = atomicAdd(trace.attempted_records, 1);
    if (index >= min(trace.capacity, 4u)) {
        atomicAdd(trace.dropped_records, 1);
        return;
    }
    trace.records[index].invocation = uvec4(trace.frame, trace.draw, uvec2(gl_FragCoord.xy));
    // The fixture is one triangle, one draw, one sample. Applications with an
    // unknown primitive use 0xffffffff rather than inventing an identifier.
    trace.records[index].selector = uvec4(0, 0, probe, components);
    trace.records[index].bits = bits;
}

void main() {
    output_color = vec4(vertex_color * palette.tint.rgb, 1.0);
    observe(0, uvec4(floatBitsToUint(gl_FragCoord.xy), 0, 0), 2);
    observe(1, uvec4(floatBitsToUint(vertex_color), 0), 3);
    observe(2, floatBitsToUint(palette.tint), 4);
    observe(3, floatBitsToUint(output_color), 4);
}
