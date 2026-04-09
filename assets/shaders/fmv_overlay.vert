#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform vec2 uPosition;  // NDC offset
uniform vec2 uScale;     // NDC scale

void main() {
    vTexCoord = aTexCoord;
    vec2 pos = aPos * uScale + uPosition;
    gl_Position = vec4(pos, 0.0, 1.0);
}
