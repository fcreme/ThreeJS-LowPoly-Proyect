#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;
uniform int uFrameCount;
uniform float uAlpha;
uniform vec2 uScrollSpeed;
uniform vec3 uTint;

void main() {
    vec2 uv = vTexCoord;

    // Sprite sheet frame selection (horizontal strip)
    if (uFrameCount > 1) {
        int frame = int(mod(uTime, float(uFrameCount)));
        float fw = 1.0 / float(uFrameCount);
        uv.x = uv.x * fw + float(frame) * fw;
    }

    // UV scrolling for continuous drift effects
    uv += uScrollSpeed * uTime;

    vec4 color = texture(uTexture, uv);
    FragColor = vec4(color.rgb * uTint, color.a * uAlpha);
}
