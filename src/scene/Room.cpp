#include "scene/Room.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Minimal JSON parsing — enough for room definitions
// (avoids pulling in a full JSON lib for now)
namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\"");
    size_t end = s.find_last_not_of(" \t\n\r\"");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

float parseFloat(const std::string& s) {
    try { return std::stof(trim(s)); }
    catch (...) { return 0.0f; }
}

glm::vec3 parseVec3(const std::string& s) {
    // Expects "[x, y, z]"
    glm::vec3 v(0.0f);
    std::string inner = s;
    size_t a = inner.find('[');
    size_t b = inner.find(']');
    if (a != std::string::npos && b != std::string::npos) {
        inner = inner.substr(a + 1, b - a - 1);
    }
    std::istringstream iss(inner);
    char comma;
    iss >> v.x >> comma >> v.y >> comma >> v.z;
    return v;
}

glm::vec2 parseVec2(const std::string& s) {
    glm::vec2 v(0.0f);
    std::string inner = s;
    size_t a = inner.find('[');
    size_t b = inner.find(']');
    if (a != std::string::npos && b != std::string::npos) {
        inner = inner.substr(a + 1, b - a - 1);
    }
    std::istringstream iss(inner);
    char comma;
    iss >> v.x >> comma >> v.y;
    return v;
}

glm::vec4 parseVec4(const std::string& s) {
    glm::vec4 v(0.0f);
    std::string inner = s;
    size_t a = inner.find('[');
    size_t b = inner.find(']');
    if (a != std::string::npos && b != std::string::npos) {
        inner = inner.substr(a + 1, b - a - 1);
    }
    std::istringstream iss(inner);
    char comma;
    iss >> v.x >> comma >> v.y >> comma >> v.z >> comma >> v.w;
    return v;
}

int parseInt(const std::string& s) {
    try { return std::stoi(trim(s)); }
    catch (...) { return 0; }
}

// Extract a field value from a JSON-like object substring
std::string getField(const std::string& obj, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = obj.find(search);
    if (pos == std::string::npos) return "";
    pos = obj.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\n' || obj[pos] == '\r')) pos++;
    if (pos >= obj.size()) return "";

    if (obj[pos] == '[') {
        size_t end = obj.find(']', pos);
        if (end == std::string::npos) return "";
        return obj.substr(pos, end - pos + 1);
    } else if (obj[pos] == '"') {
        size_t end = obj.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return obj.substr(pos + 1, end - pos - 1);
    } else {
        size_t end = obj.find_first_of(",}\n\r", pos);
        if (end == std::string::npos) end = obj.size();
        return trim(obj.substr(pos, end - pos));
    }
}

} // anonymous namespace

namespace dw {

bool loadRoomDef(const std::string& path, RoomDef& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Room: cannot open: " << path << "\n";
        return false;
    }

    // Simple key-value parsing from JSON-like format
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    auto getValue = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return "";
        pos = content.find(':', pos);
        if (pos == std::string::npos) return "";
        pos++;
        // Skip whitespace
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) pos++;
        if (pos >= content.size()) return "";

        if (content[pos] == '[') {
            // Array value
            size_t end = content.find(']', pos);
            if (end == std::string::npos) return "";
            return content.substr(pos, end - pos + 1);
        } else if (content[pos] == '"') {
            // String value
            size_t end = content.find('"', pos + 1);
            if (end == std::string::npos) return "";
            return content.substr(pos + 1, end - pos - 1);
        } else {
            // Number or bool
            size_t end = content.find_first_of(",}\n", pos);
            if (end == std::string::npos) end = content.size();
            return trim(content.substr(pos, end - pos));
        }
    };

    out.name = getValue("name");
    out.backgroundPath = getValue("background");
    out.collisionPath = getValue("collision");
    out.depthGeometryPath = getValue("depth_geometry");
    out.ambientAudioPath = getValue("ambient_audio");

    std::string fp = getValue("first_person");
    out.firstPerson = (fp == "true");

    std::string camPos = getValue("camera_position");
    if (!camPos.empty()) out.cameraPos = parseVec3(camPos);

    std::string camTarget = getValue("camera_target");
    if (!camTarget.empty()) out.cameraTarget = parseVec3(camTarget);

    std::string fov = getValue("camera_fov");
    if (!fov.empty()) out.cameraFov = parseFloat(fov);

    std::string amb = getValue("ambient");
    if (!amb.empty()) out.ambientColor = parseVec3(amb);

    std::string ldir = getValue("light_direction");
    if (!ldir.empty()) out.lightDir = parseVec3(ldir);

    std::string lcol = getValue("light_color");
    if (!lcol.empty()) out.lightColor = parseVec3(lcol);

    // Parse particles array
    {
        std::string pKey = "\"particles\"";
        size_t pPos = content.find(pKey);
        if (pPos != std::string::npos) {
            // Find the opening '[' of the particles array
            size_t arrStart = content.find('[', pPos + pKey.size());
            if (arrStart != std::string::npos) {
                // Find each {...} object inside the array
                size_t searchPos = arrStart + 1;
                while (true) {
                    size_t objStart = content.find('{', searchPos);
                    if (objStart == std::string::npos) break;
                    // Check we haven't passed the array's closing ']'
                    size_t arrEnd = content.find(']', searchPos);
                    if (arrEnd != std::string::npos && objStart > arrEnd) break;

                    size_t objEnd = content.find('}', objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = content.substr(objStart, objEnd - objStart + 1);

                    ParticleEmitterDef pdef;
                    pdef.type = getField(obj, "type");

                    std::string orig = getField(obj, "origin");
                    if (!orig.empty()) pdef.origin = parseVec3(orig);

                    std::string ar = getField(obj, "area");
                    if (!ar.empty()) pdef.area = parseVec3(ar);

                    std::string cnt = getField(obj, "count");
                    if (!cnt.empty()) pdef.count = parseInt(cnt);

                    std::string col = getField(obj, "color");
                    if (!col.empty()) pdef.color = parseVec4(col);

                    std::string spd = getField(obj, "speed");
                    if (!spd.empty()) pdef.speed = parseFloat(spd);

                    std::string bs = getField(obj, "base_size");
                    if (!bs.empty()) pdef.baseSize = parseFloat(bs);

                    out.particles.push_back(pdef);
                    searchPos = objEnd + 1;
                }
            }
        }
    }

    // Parse fmv_overlays array
    {
        std::string pKey = "\"fmv_overlays\"";
        size_t pPos = content.find(pKey);
        if (pPos != std::string::npos) {
            size_t arrStart = content.find('[', pPos + pKey.size());
            if (arrStart != std::string::npos) {
                size_t searchPos = arrStart + 1;
                while (true) {
                    size_t objStart = content.find('{', searchPos);
                    if (objStart == std::string::npos) break;
                    size_t arrEnd = content.find(']', searchPos);
                    if (arrEnd != std::string::npos && objStart > arrEnd) break;

                    size_t objEnd = content.find('}', objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = content.substr(objStart, objEnd - objStart + 1);

                    FMVOverlayDef fdef;
                    fdef.type = getField(obj, "type");
                    fdef.imagePath = getField(obj, "image");

                    std::string pos = getField(obj, "position");
                    if (!pos.empty()) fdef.position = parseVec2(pos);

                    std::string scl = getField(obj, "scale");
                    if (!scl.empty()) fdef.scale = parseVec2(scl);

                    std::string al = getField(obj, "alpha");
                    if (!al.empty()) fdef.alpha = parseFloat(al);

                    std::string spd = getField(obj, "speed");
                    if (!spd.empty()) fdef.speed = parseFloat(spd);

                    std::string fc = getField(obj, "frame_count");
                    if (!fc.empty()) fdef.frameCount = parseInt(fc);

                    std::string add = getField(obj, "additive");
                    if (!add.empty()) fdef.additive = (add == "true");

                    std::string ss = getField(obj, "scroll_speed");
                    if (!ss.empty()) fdef.scrollSpeed = parseVec2(ss);

                    std::string tnt = getField(obj, "tint");
                    if (!tnt.empty()) fdef.tint = parseVec3(tnt);

                    out.fmvOverlays.push_back(fdef);
                    searchPos = objEnd + 1;
                }
            }
        }
    }

    std::cout << "Room: loaded '" << out.name << "' from " << path
              << " (" << out.particles.size() << " particle emitters, "
              << out.fmvOverlays.size() << " FMV overlays)\n";
    return true;
}

} // namespace dw
