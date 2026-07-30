#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in float coverage;

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
} ubuf;

layout(location = 0) out vec4 vColor;
layout(location = 1) out float vCoverage;

void main() {
    vColor = color;
    vCoverage = coverage;
    gl_Position = ubuf.matrix * vec4(position, 0.0, 1.0);
}
