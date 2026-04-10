#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vFragPosLightSpace;
in mat3 vTBN;

out vec4 FragColor;

uniform sampler2D uDiffuse;
uniform bool uHasTexture;

// Normal mapping
uniform sampler2D uNormalMap;
uniform int uHasNormalMap;

// Directional light
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;

// Point lights (up to 8)
#define MAX_POINT_LIGHTS 8
uniform int uNumPointLights;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightRadius[MAX_POINT_LIGHTS];

// Material color (used when no texture)
uniform vec3 uMaterialColor;

// Camera position for specular
uniform vec3 uViewPos;

// Material roughness (0 = mirror, 1 = matte)
uniform float uRoughness;

// Emissive
uniform vec3 uEmissive;

// Interaction highlight (0 = none, 1 = full glow)
uniform float uHighlight;

// Shadows
uniform sampler2D uShadowMap;
uniform int uUseShadows;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal) {
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Fragment outside shadow map — no shadow
    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    // Bias based on surface angle to light, scaled by texel size
    vec3 lightDir = normalize(-uLightDir);
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    // PCF 5x5 with Poisson disk sampling for soft shadow edges
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    const vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201, -0.39906), vec2( 0.94558, -0.76890),
        vec2(-0.09418, -0.92938), vec2( 0.34495,  0.29387),
        vec2(-0.91588, -0.45771), vec2(-0.81544,  0.48568),
        vec2(-0.38277, -0.56870), vec2( 0.44323, -0.97511),
        vec2( 0.53742,  0.01228), vec2( 0.72843, -0.17972),
        vec2(-0.69710,  0.32609), vec2( 0.18908, -0.55507),
        vec2(-0.23237,  0.12544), vec2( 0.04795,  0.86584),
        vec2(-0.57734, -0.00453), vec2( 0.67516,  0.60655)
    );
    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        float pcfDepth = texture(uShadowMap, projCoords.xy + poissonDisk[i] * texelSize * 2.0).r;
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
    }
    shadow /= 16.0;

    // 70% shadow intensity (not full black)
    return shadow * 0.7;
}

void main() {
    // Determine surface normal (with or without normal map)
    vec3 normal;
    if (uHasNormalMap != 0) {
        vec3 mapNormal = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        normal = normalize(vTBN * mapNormal);
    } else {
        normal = normalize(vNormal);
    }
    if (!gl_FrontFacing) normal = -normal; // flip normal for back-faces (interior walls)

    vec3 lightDir = normalize(-uLightDir);

    // Base color
    vec3 baseColor;
    if (uHasTexture) {
        baseColor = texture(uDiffuse, vTexCoord).rgb;
    } else {
        baseColor = uMaterialColor;
    }

    // Directional light contribution
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    float diff = max(dot(normal, lightDir), 0.0);

    // Blinn-Phong specular for directional light
    float shininess = pow(2.0, (1.0 - uRoughness) * 10.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);

    // Shadow attenuation on directional light
    float shadow = 0.0;
    if (uUseShadows != 0) {
        shadow = calculateShadow(vFragPosLightSpace, normal);
    }

    vec3 result = uAmbient * baseColor
                + (1.0 - shadow) * (diff * uLightColor * baseColor + spec * uLightColor * 0.25);

    // Point light contributions (not affected by directional shadow map)
    for (int i = 0; i < uNumPointLights && i < MAX_POINT_LIGHTS; i++) {
        vec3 toLight = uPointLightPos[i] - vWorldPos;
        float dist = length(toLight);
        vec3 lightVec = toLight / dist;

        // Attenuation (smooth falloff)
        float atten = clamp(1.0 - dist / uPointLightRadius[i], 0.0, 1.0);
        atten *= atten; // quadratic falloff

        float ndotl = max(dot(normal, lightVec), 0.0);

        // Blinn-Phong specular for point lights
        vec3 ptHalf = normalize(lightVec + viewDir);
        float ptSpec = pow(max(dot(normal, ptHalf), 0.0), shininess);

        result += atten * (ndotl * uPointLightColor[i] * baseColor + ptSpec * uPointLightColor[i] * 0.15);
    }

    // Emissive contribution (adds directly, feeds bloom naturally)
    result += uEmissive;

    // Interaction highlight: emissive rim glow
    if (uHighlight > 0.0) {
        // Fresnel-based rim light
        vec3 viewDir = normalize(-vWorldPos); // approximate view dir
        float rim = 1.0 - max(dot(normal, viewDir), 0.0);
        rim = pow(rim, 2.0);
        vec3 glowColor = vec3(0.9, 0.7, 0.3); // warm gold glow
        result += glowColor * rim * uHighlight * 1.5;
        // Subtle overall emissive boost
        result += baseColor * uHighlight * 0.15;
    }

    FragColor = vec4(result, 1.0);
}
