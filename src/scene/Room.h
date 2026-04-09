#pragma once

#include "renderer/Camera.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace dw {

struct RoomPropDef {
    std::string modelPath;
    std::string texturePath;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};  // euler degrees
    glm::vec3 scale{1.0f};
    bool interactive = false;
};

struct PointLightDef {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float radius = 5.0f;
    bool flicker = false;
};

struct ParticleEmitterDef {
    std::string type;  // "dust", "fog", "fireflies", "fire"
    glm::vec3 origin{0.0f};
    glm::vec3 area{5.0f, 3.0f, 5.0f};
    int count = 100;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 0.3f};
    float speed = 0.3f;
    float baseSize = 3.0f;
};

struct FMVOverlayDef {
    std::string type;           // "fog", "fire_glow", "light_shaft", "custom"
    std::string imagePath;      // sprite sheet path (empty = procedural)
    glm::vec2 position{0.0f};  // NDC offset
    glm::vec2 scale{1.0f};     // NDC scale
    float alpha = 0.5f;
    float speed = 1.0f;        // animation speed multiplier
    int frameCount = 1;         // sprite sheet frames
    bool additive = false;      // additive vs alpha blend
    glm::vec2 scrollSpeed{0.0f}; // UV scroll per second
    glm::vec3 tint{1.0f};      // color tint
};

struct RoomDef {
    std::string name;
    std::string backgroundPath;
    std::string collisionPath;
    std::string ambientAudioPath;
    std::string depthGeometryPath;

    bool firstPerson = false;

    // Camera
    glm::vec3 cameraPos{0.0f, 2.0f, 5.0f};
    glm::vec3 cameraTarget{0.0f, 0.0f, 0.0f};
    float cameraFov = 45.0f;

    // Props
    std::vector<RoomPropDef> props;

    // Lighting
    glm::vec3 ambientColor{0.1f, 0.1f, 0.12f};
    glm::vec3 lightDir{-0.5f, -1.0f, -0.3f};
    glm::vec3 lightColor{0.8f, 0.75f, 0.7f};
    std::vector<PointLightDef> pointLights;

    // Particles
    std::vector<ParticleEmitterDef> particles;

    // FMV overlays (animated atmospheric effects on background)
    std::vector<FMVOverlayDef> fmvOverlays;
};

bool loadRoomDef(const std::string& path, RoomDef& out);

} // namespace dw
