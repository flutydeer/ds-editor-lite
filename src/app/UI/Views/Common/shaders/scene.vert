#version 440

layout(location = 0) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf {
    int flip;
} ubuf;

void main() {
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vTexCoord = position;
    if (ubuf.flip != 0)
        vTexCoord.y = 1.0 - vTexCoord.y;
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
