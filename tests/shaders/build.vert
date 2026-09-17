#version 460

// CPU compiler probe, not a rendering scenario or an expected diagnosis.
layout(location = 0) in vec2 position;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
}
