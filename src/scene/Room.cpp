#include "scene/Room.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace {

glm::vec2 parseVec2(const json& j) {
    return glm::vec2(j[0].get<float>(), j[1].get<float>());
}

glm::vec3 parseVec3(const json& j) {
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

glm::vec4 parseVec4(const json& j) {
    return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

template<typename T>
T getOr(const json& j, const std::string& key, const T& def) {
    if (j.contains(key)) return j[key].get<T>();
    return def;
}

} // anonymous namespace

namespace dw {

bool loadRoomDef(const std::string& path, RoomDef& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Room: cannot open: " << path << "\n";
        return false;
    }

    json j;
    try {
        j = json::parse(file);
    } catch (const json::parse_error& e) {
        std::cerr << "Room: JSON parse error in " << path << ": " << e.what() << "\n";
        return false;
    }

    out.name = getOr<std::string>(j, "name", "");
    out.backgroundPath = getOr<std::string>(j, "background", "");
    out.collisionPath = getOr<std::string>(j, "collision", "");
    out.depthGeometryPath = getOr<std::string>(j, "depth_geometry", "");
    out.ambientAudioPath = getOr<std::string>(j, "ambient_audio", "");
    out.firstPerson = getOr<bool>(j, "first_person", false);

    // Player
    out.eyeHeight = getOr<float>(j, "eye_height", 1.7f);
    if (j.contains("player_spawn")) out.playerSpawn = parseVec3(j["player_spawn"]);

    // World bounds
    if (j.contains("world_bounds")) {
        auto& wb = j["world_bounds"];
        out.boundsMinX = wb["min"][0].get<float>();
        out.boundsMinZ = wb["min"][1].get<float>();
        out.boundsMaxX = wb["max"][0].get<float>();
        out.boundsMaxZ = wb["max"][1].get<float>();
    }

    // Collision boxes
    if (j.contains("collision_boxes")) {
        for (const auto& cb : j["collision_boxes"]) {
            CollisionBox box;
            box.min = parseVec2(cb["min"]);
            box.max = parseVec2(cb["max"]);
            out.collisionBoxes.push_back(box);
        }
    }

    // Reverb
    if (j.contains("reverb")) {
        auto& rv = j["reverb"];
        out.reverb.enabled = getOr<bool>(rv, "enabled", false);
        out.reverb.feedback = getOr<float>(rv, "feedback", 0.15f);
        out.reverb.delayMs = getOr<float>(rv, "delay_ms", 300.0f);
    }

    // Camera
    if (j.contains("camera_position")) out.cameraPos = parseVec3(j["camera_position"]);
    if (j.contains("camera_target")) out.cameraTarget = parseVec3(j["camera_target"]);
    out.cameraFov = getOr<float>(j, "camera_fov", 45.0f);

    // Lighting
    if (j.contains("ambient")) out.ambientColor = parseVec3(j["ambient"]);
    if (j.contains("light_direction")) out.lightDir = parseVec3(j["light_direction"]);
    if (j.contains("light_color")) out.lightColor = parseVec3(j["light_color"]);

    // Point lights
    if (j.contains("point_lights")) {
        for (const auto& pl : j["point_lights"]) {
            PointLightDef ld;
            if (pl.contains("position")) ld.position = parseVec3(pl["position"]);
            if (pl.contains("color")) ld.color = parseVec3(pl["color"]);
            ld.radius = getOr<float>(pl, "radius", 5.0f);
            ld.baseRadius = ld.radius;
            ld.flicker = getOr<bool>(pl, "flicker", false);
            out.pointLights.push_back(ld);
        }
    }

    // Props
    if (j.contains("props")) {
        for (const auto& pr : j["props"]) {
            RoomPropDef pd;
            pd.modelPath = getOr<std::string>(pr, "model", "");
            pd.texturePath = getOr<std::string>(pr, "texture", "");
            if (pr.contains("position")) pd.position = parseVec3(pr["position"]);
            if (pr.contains("rotation")) pd.rotation = parseVec3(pr["rotation"]);
            if (pr.contains("scale")) pd.scale = parseVec3(pr["scale"]);
            pd.interactive = getOr<bool>(pr, "interactive", false);
            out.props.push_back(pd);
        }
    }

    // Particles
    if (j.contains("particles")) {
        for (const auto& pe : j["particles"]) {
            ParticleEmitterDef pdef;
            pdef.type = getOr<std::string>(pe, "type", "dust");
            if (pe.contains("origin")) pdef.origin = parseVec3(pe["origin"]);
            if (pe.contains("area")) pdef.area = parseVec3(pe["area"]);
            pdef.count = getOr<int>(pe, "count", 100);
            if (pe.contains("color")) pdef.color = parseVec4(pe["color"]);
            pdef.speed = getOr<float>(pe, "speed", 0.3f);
            pdef.baseSize = getOr<float>(pe, "base_size", 3.0f);
            out.particles.push_back(pdef);
        }
    }

    // FMV overlays
    if (j.contains("fmv_overlays")) {
        for (const auto& fo : j["fmv_overlays"]) {
            FMVOverlayDef fdef;
            fdef.type = getOr<std::string>(fo, "type", "fog");
            fdef.imagePath = getOr<std::string>(fo, "image", "");
            if (fo.contains("position")) fdef.position = parseVec2(fo["position"]);
            if (fo.contains("scale")) fdef.scale = parseVec2(fo["scale"]);
            fdef.alpha = getOr<float>(fo, "alpha", 0.5f);
            fdef.speed = getOr<float>(fo, "speed", 1.0f);
            fdef.frameCount = getOr<int>(fo, "frame_count", 1);
            fdef.additive = getOr<bool>(fo, "additive", false);
            if (fo.contains("scroll_speed")) fdef.scrollSpeed = parseVec2(fo["scroll_speed"]);
            if (fo.contains("tint")) fdef.tint = parseVec3(fo["tint"]);
            out.fmvOverlays.push_back(fdef);
        }
    }

    std::cout << "Room: loaded '" << out.name << "' from " << path
              << " (" << out.particles.size() << " particle emitters, "
              << out.fmvOverlays.size() << " FMV overlays, "
              << out.collisionBoxes.size() << " collision boxes)\n";
    return true;
}

} // namespace dw
