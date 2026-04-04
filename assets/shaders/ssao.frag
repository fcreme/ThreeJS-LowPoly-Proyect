#version 330 core

in vec2 vTexCoord;
out float FragColor;

uniform sampler2D uDepthTex;
uniform sampler2D uNoiseTex;

uniform vec3 uSamples[32];
uniform mat4 uProjection;
uniform mat4 uInvProjection;
uniform vec2 uNoiseScale;
uniform float uRadius;
uniform float uBias;
uniform float uNearPlane;
uniform float uFarPlane;

// Reconstruct view-space position from depth
vec3 viewPosFromDepth(vec2 uv, float depth) {
    // NDC
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

// Reconstruct normal from depth buffer (cross product of neighbors)
vec3 reconstructNormal(vec3 viewPos, vec2 uv) {
    vec2 texelSize = 1.0 / textureSize(uDepthTex, 0);
    float depthR = texture(uDepthTex, uv + vec2(texelSize.x, 0.0)).r;
    float depthU = texture(uDepthTex, uv + vec2(0.0, texelSize.y)).r;
    vec3 posR = viewPosFromDepth(uv + vec2(texelSize.x, 0.0), depthR);
    vec3 posU = viewPosFromDepth(uv + vec2(0.0, texelSize.y), depthU);
    return normalize(cross(posR - viewPos, posU - viewPos));
}

void main() {
    float depth = texture(uDepthTex, vTexCoord).r;

    // Skip sky/far plane
    if (depth >= 1.0) {
        FragColor = 1.0;
        return;
    }

    vec3 fragPos = viewPosFromDepth(vTexCoord, depth);
    vec3 normal = reconstructNormal(fragPos, vTexCoord);

    // Random rotation vector from noise texture (already in [-1,1] range, z=0)
    vec3 randomVec = texture(uNoiseTex, vTexCoord * uNoiseScale).xyz;

    // Gram-Schmidt to build TBN
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    int sampleCount = 32;

    for (int i = 0; i < sampleCount; i++) {
        // Sample position in view space
        vec3 samplePos = fragPos + TBN * uSamples[i] * uRadius;

        // Project to screen
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Sample depth at that screen position
        float sampleDepth = texture(uDepthTex, offset.xy).r;
        vec3 sampleViewPos = viewPosFromDepth(offset.xy, sampleDepth);

        // Range check + occlusion test
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleViewPos.z));
        occlusion += (sampleViewPos.z >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(sampleCount));
    // Power curve to intensify AO for dark atmospheric look
    FragColor = pow(occlusion, 1.5);
}
