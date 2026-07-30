#version 440

layout(location = 0) in vec4 vColor;
layout(location = 1) in float vCoverage;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vColor * clamp(vCoverage, 0.0, 1.0);
}
