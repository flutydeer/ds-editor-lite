#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 textureCoordinate;

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 vertexTextureCoordinate;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 matrix;
};

void main()
{
    vertexColor = color;
    vertexTextureCoordinate = textureCoordinate;
    gl_Position = matrix * vec4(position, 0.0, 1.0);
}
