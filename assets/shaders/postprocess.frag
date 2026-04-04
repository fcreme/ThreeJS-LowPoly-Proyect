#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uScreen;
uniform sampler2D uBloomTex;
uniform sampler2D uDepthTex;
uniform sampler2D uSSAOTex;
uniform float uTime;
uniform float uBloomIntensity;
uniform vec2 uResolution;

// SSAO
uniform int uSSAOEnabled;

// Distance fog
uniform int uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uNearPlane;
uniform float uFarPlane;

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float linearizeDepth(float d, float near, float far) {
    return (2.0 * near * far) / (far + near - (d * 2.0 - 1.0) * (far - near));
}

void main() {
    // --- Chromatic aberration ---
    // Offset increases toward screen edges for a lens-like distortion
    vec2 center = vTexCoord - 0.5;
    float edgeDist = dot(center, center); // squared distance from center
    float caStrength = edgeDist * 0.008;  // subtle — ramps at edges only
    vec3 color;
    color.r = texture(uScreen, vTexCoord + center * caStrength).r;
    color.g = texture(uScreen, vTexCoord).g;
    color.b = texture(uScreen, vTexCoord - center * caStrength).b;

    // --- SSAO ---
    if (uSSAOEnabled != 0) {
        float ao = texture(uSSAOTex, vTexCoord).r;
        color *= ao;
    }

    // --- Bloom ---
    vec3 bloom = texture(uBloomTex, vTexCoord).rgb;
    color += bloom * uBloomIntensity;

    // --- Distance Fog ---
    if (uFogEnabled != 0) {
        float depth = texture(uDepthTex, vTexCoord).r;
        float linDepth = linearizeDepth(depth, uNearPlane, uFarPlane);
        float fogFactor = smoothstep(uFogStart, uFogEnd, linDepth);
        color = mix(color, uFogColor, fogFactor);
    }

    // --- Vignette ---
    vec2 uv = vTexCoord;
    float dist = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.7, 0.4, dist);
    color *= mix(0.3, 1.0, vignette);

    // --- Film grain ---
    float grain = rand(vTexCoord * uResolution + vec2(uTime * 100.0)) * 0.08;
    color += grain - 0.04;

    // --- Subtle color grading (push toward cold tones) ---
    color.r *= 0.95;
    color.b *= 1.05;

    // --- Slight contrast boost ---
    color = (color - 0.5) * 1.1 + 0.5;

    // --- Dithering (eliminates banding in dark gradients) ---
    // Triangular-distributed dither: smoother than uniform noise
    float dither1 = rand(vTexCoord * uResolution + vec2(uTime * 37.0));
    float dither2 = rand(vTexCoord * uResolution + vec2(uTime * 71.0 + 0.5));
    vec3 dither = vec3((dither1 + dither2 - 1.0) / 255.0);
    color += dither;

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
