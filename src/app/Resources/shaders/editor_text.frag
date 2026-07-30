#version 440

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 fragmentColor;

layout(binding = 1) uniform sampler2D atlasTexture;

void main()
{
    float coverage = texture(atlasTexture, vertexTextureCoordinate).a;
    fragmentColor = vertexColor * coverage;
}
