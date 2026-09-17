#version 450

layout(location = 0) out vec3 vertex_color;

const vec2 positions[3] = vec2[3](vec2(-0.8, -0.75), vec2(0.8, -0.75), vec2(0.0, 0.75));
const vec3 colors[3] = vec3[3](vec3(1.0, 0.125, 0.0625),
                             vec3(0.125, 1.0, 0.0625),
                             vec3(0.125, 0.0625, 1.0));

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    vertex_color = colors[gl_VertexIndex];
}
