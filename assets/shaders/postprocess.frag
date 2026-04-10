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
    // Aspect-correct the UV so strength is consistent at any resolution
    vec2 center = vTexCoord - 0.5;
    vec2 aspect = vec2(uResolution.x / uResolution.y, 1.0);
    float edgeDist = dot(center * aspect, center * aspect);
    float caStrength = edgeDist * 0.006;
    vec3 color;
    color.r = texture(uScreen, vTexCoord + center * caStrength * 1.2).r;
    color.g = texture(uScreen, vTexCoord).g;
    color.b = texture(uScreen, vTexCoord - center * caStrength * 0.8).b;

    // --- Detect background pixels (no depth written) ---
    float depth = texture(uDepthTex, vTexCoord).r;
    bool isBackground = (depth > 0.95);

    // --- SSAO ---
    if (uSSAOEnabled != 0 && !isBackground) {
        float ao = texture(uSSAOTex, vTexCoord).r;
        ao = pow(ao, 1.2); // deepen ambient occlusion
        color *= ao;
    }

    // --- Bloom ---
    vec3 bloom = texture(uBloomTex, vTexCoord).rgb;
    color += bloom * uBloomIntensity;

    // --- Distance Fog ---
    if (uFogEnabled != 0 && !isBackground) {
        float linDepth = linearizeDepth(depth, uNearPlane, uFarPlane);
        float fogFactor = 1.0 - exp(-pow(linDepth / uFogEnd, 2.0) * uFogStart);
        color = mix(color, uFogColor, fogFactor);
    }

    // --- Vignette (aspect-correct, applied globally) ---
    {
        vec2 uv = (vTexCoord - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
        float dist = length(uv);
        float vignette = smoothstep(0.7, 0.4, dist);
        color *= mix(0.5, 1.0, vignette);
    }

    // --- Film grain ---
    // Spatiotemporal noise: spatial hash + slow time drift to reduce shimmer
    float grain = rand(floor(vTexCoord * uResolution) + vec2(floor(uTime * 8.0))) * 0.03;
    color += grain - 0.015;

    // --- Color grading: desaturation + cold tint for cinematic look ---
    {
        float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = mix(color, vec3(lum), 0.15); // 15% desaturation
        color *= vec3(0.92, 0.96, 1.08);     // cold blue tint
    }

    // --- Contrast boost (applied globally to avoid 3D/background seam) ---
    color = (color - 0.5) * 1.15 + 0.5;

    // --- ACES filmic tone mapping ---
    // Preserves highlight rolloff and cinematic contrast from HDR values
    {
        vec3 x = color;
        float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
        color = clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
    }

    // --- Dithering (eliminates banding in dark gradients) ---
    // Triangular-distributed dither scaled to one 8-bit LSB (1/255)
    float dither1 = rand(floor(vTexCoord * uResolution) + vec2(floor(uTime * 4.0) * 37.0));
    float dither2 = rand(floor(vTexCoord * uResolution) + vec2(floor(uTime * 4.0) * 71.0 + 0.5));
    vec3 dither = vec3(dither1 + dither2 - 1.0) / 255.0;
    color += dither;

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
