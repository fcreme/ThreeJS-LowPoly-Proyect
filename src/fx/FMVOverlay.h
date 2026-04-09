#pragma once

#include "scene/Room.h"
#include "renderer/Shader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace dw {

class FMVOverlay {
public:
    FMVOverlay() = default;
    ~FMVOverlay();

    FMVOverlay(FMVOverlay&& other) noexcept;
    FMVOverlay& operator=(FMVOverlay&& other) noexcept;

    FMVOverlay(const FMVOverlay&) = delete;
    FMVOverlay& operator=(const FMVOverlay&) = delete;

    static void initShared();
    static void shutdownShared();

    void loadFromDef(const FMVOverlayDef& def);
    void update(float dt);
    void render();

private:
    static GLuint generateProceduralTexture(const std::string& type, int frameCount);

    GLuint m_texture = 0;
    FMVOverlayDef m_def;
    float m_time = 0.0f;

    static Shader s_shader;
    static GLuint s_vao;
    static GLuint s_vbo;
    static bool s_initialized;
};

} // namespace dw
