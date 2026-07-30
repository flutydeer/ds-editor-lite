#version 440

layout(binding = 1) uniform sampler2D atlas;
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

void main() {
    float coverage = texture(atlas, vTexCoord).a;
    fragColor = vColor * coverage;
}
