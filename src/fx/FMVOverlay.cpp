#include "fx/FMVOverlay.h"

#include <glad/gl.h>
#include <iostream>
#include <cmath>
#include <random>
#include <vector>

#include "stb_image.h"

namespace dw {

Shader FMVOverlay::s_shader;
GLuint FMVOverlay::s_vao = 0;
GLuint FMVOverlay::s_vbo = 0;
bool FMVOverlay::s_initialized = false;

FMVOverlay::~FMVOverlay() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
}

FMVOverlay::FMVOverlay(FMVOverlay&& other) noexcept
    : m_texture(other.m_texture), m_def(std::move(other.m_def)), m_time(other.m_time) {
    other.m_texture = 0;
}

FMVOverlay& FMVOverlay::operator=(FMVOverlay&& other) noexcept {
    if (this != &other) {
        if (m_texture != 0) glDeleteTextures(1, &m_texture);
        m_texture = other.m_texture;
        m_def = std::move(other.m_def);
        m_time = other.m_time;
        other.m_texture = 0;
    }
    return *this;
}

void FMVOverlay::initShared() {
    if (s_initialized) return;

    if (!s_shader.loadFromFile("assets/shaders/fmv_overlay.vert", "assets/shaders/fmv_overlay.frag")) {
        std::cerr << "FMVOverlay: failed to load shader\n";
        return;
    }

    // Fullscreen quad: position (x,y) + texcoord (u,v)
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    s_initialized = true;
    std::cout << "FMVOverlay: shared resources initialized\n";
}

void FMVOverlay::shutdownShared() {
    if (!s_initialized) return;
    if (s_vbo != 0) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao != 0) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    s_shader = Shader(); // destroy via move
    s_initialized = false;
}

void FMVOverlay::loadFromDef(const FMVOverlayDef& def) {
    m_def = def;
    m_time = 0.0f;

    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }

    if (!def.imagePath.empty()) {
        // Load sprite sheet from file
        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(def.imagePath.c_str(), &width, &height, &channels, 4);
        if (data) {
            glGenTextures(1, &m_texture);
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            stbi_image_free(data);
            std::cout << "FMVOverlay: loaded sprite sheet " << def.imagePath
                      << " (" << width << "x" << height << ")\n";
        } else {
            std::cerr << "FMVOverlay: failed to load " << def.imagePath << ", using procedural\n";
            m_texture = generateProceduralTexture(def.type, def.frameCount);
        }
    } else {
        m_texture = generateProceduralTexture(def.type, def.frameCount);
    }
}

void FMVOverlay::update(float dt) {
    m_time += dt * m_def.speed;
}

void FMVOverlay::render() {
    if (!s_initialized || m_texture == 0) return;

    // Set blend mode
    if (m_def.additive) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    s_shader.use();
    s_shader.setVec2("uPosition", m_def.position.x, m_def.position.y);
    s_shader.setVec2("uScale", m_def.scale.x, m_def.scale.y);
    s_shader.setFloat("uTime", m_time);
    s_shader.setInt("uFrameCount", m_def.frameCount);
    s_shader.setFloat("uAlpha", m_def.alpha);
    s_shader.setVec2("uScrollSpeed", m_def.scrollSpeed.x, m_def.scrollSpeed.y);
    s_shader.setVec3("uTint", m_def.tint.x, m_def.tint.y, m_def.tint.z);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    s_shader.setInt("uTexture", 0);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Restore default blend mode
    if (m_def.additive) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

GLuint FMVOverlay::generateProceduralTexture(const std::string& type, int frameCount) {
    frameCount = std::max(1, frameCount);
    int frameW = 128;
    int frameH = 128;
    int totalW = frameW * frameCount;

    std::vector<unsigned char> pixels(totalW * frameH * 4, 0);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (type == "fog") {
        // Soft noise blobs per frame
        for (int f = 0; f < frameCount; f++) {
            float phase = static_cast<float>(f) / frameCount;
            // Generate a few blob centers
            float cx1 = 0.3f + phase * 0.4f, cy1 = 0.4f;
            float cx2 = 0.7f - phase * 0.3f, cy2 = 0.6f;
            float cx3 = 0.5f, cy3 = 0.3f + phase * 0.2f;

            for (int y = 0; y < frameH; y++) {
                for (int x = 0; x < frameW; x++) {
                    float u = static_cast<float>(x) / frameW;
                    float v = static_cast<float>(y) / frameH;

                    float d1 = std::sqrt((u - cx1) * (u - cx1) + (v - cy1) * (v - cy1));
                    float d2 = std::sqrt((u - cx2) * (u - cx2) + (v - cy2) * (v - cy2));
                    float d3 = std::sqrt((u - cx3) * (u - cx3) + (v - cy3) * (v - cy3));

                    float val = std::exp(-d1 * 4.0f) * 0.5f
                              + std::exp(-d2 * 3.5f) * 0.4f
                              + std::exp(-d3 * 5.0f) * 0.3f;

                    // Edge fadeout
                    float edgeX = std::min(u, 1.0f - u) * 4.0f;
                    float edgeY = std::min(v, 1.0f - v) * 4.0f;
                    float edge = glm::clamp(std::min(edgeX, edgeY), 0.0f, 1.0f);
                    val *= edge;

                    val = glm::clamp(val, 0.0f, 1.0f);
                    int idx = (y * totalW + f * frameW + x) * 4;
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 255;
                    pixels[idx + 2] = 255;
                    pixels[idx + 3] = static_cast<unsigned char>(val * 255);
                }
            }
        }
    } else if (type == "fire_glow") {
        // Warm radial gradients that pulse per frame
        for (int f = 0; f < frameCount; f++) {
            float phase = static_cast<float>(f) / frameCount;
            float intensity = 0.7f + 0.3f * std::sin(phase * 6.283f);

            for (int y = 0; y < frameH; y++) {
                for (int x = 0; x < frameW; x++) {
                    float u = static_cast<float>(x) / frameW;
                    float v = static_cast<float>(y) / frameH;

                    float cx = 0.5f + 0.05f * std::sin(phase * 6.283f * 2.0f);
                    float cy = 0.5f + 0.03f * std::cos(phase * 6.283f * 3.0f);
                    float d = std::sqrt((u - cx) * (u - cx) + (v - cy) * (v - cy));

                    float val = std::exp(-d * 3.0f) * intensity;
                    val = glm::clamp(val, 0.0f, 1.0f);

                    int idx = (y * totalW + f * frameW + x) * 4;
                    pixels[idx + 0] = static_cast<unsigned char>(255 * glm::clamp(val * 1.0f, 0.0f, 1.0f));
                    pixels[idx + 1] = static_cast<unsigned char>(255 * glm::clamp(val * 0.7f, 0.0f, 1.0f));
                    pixels[idx + 2] = static_cast<unsigned char>(255 * glm::clamp(val * 0.3f, 0.0f, 1.0f));
                    pixels[idx + 3] = static_cast<unsigned char>(val * 255);
                }
            }
        }
    } else if (type == "light_shaft") {
        // Vertical gradient with soft edges (single frame typically)
        for (int f = 0; f < frameCount; f++) {
            for (int y = 0; y < frameH; y++) {
                for (int x = 0; x < frameW; x++) {
                    float u = static_cast<float>(x) / frameW;
                    float v = static_cast<float>(y) / frameH;

                    // Vertical beam centered horizontally
                    float hDist = std::abs(u - 0.5f);
                    float beam = std::exp(-hDist * 8.0f);
                    // Fade from top to bottom
                    float vFade = glm::clamp(v * 1.5f, 0.0f, 1.0f);
                    float val = beam * vFade * 0.8f;

                    val = glm::clamp(val, 0.0f, 1.0f);
                    int idx = (y * totalW + f * frameW + x) * 4;
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 245;
                    pixels[idx + 2] = 220;
                    pixels[idx + 3] = static_cast<unsigned char>(val * 255);
                }
            }
        }
    } else {
        // Generic: soft white blob
        for (int f = 0; f < frameCount; f++) {
            for (int y = 0; y < frameH; y++) {
                for (int x = 0; x < frameW; x++) {
                    float u = static_cast<float>(x) / frameW;
                    float v = static_cast<float>(y) / frameH;
                    float d = std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
                    float val = glm::clamp(std::exp(-d * 4.0f) * 0.6f, 0.0f, 1.0f);
                    int idx = (y * totalW + f * frameW + x) * 4;
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 255;
                    pixels[idx + 2] = 255;
                    pixels[idx + 3] = static_cast<unsigned char>(val * 255);
                }
            }
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, totalW, frameH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    std::cout << "FMVOverlay: generated procedural '" << type << "' texture ("
              << totalW << "x" << frameH << ", " << frameCount << " frames)\n";
    return tex;
}

} // namespace dw
