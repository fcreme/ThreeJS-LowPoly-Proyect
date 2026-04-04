#pragma once

#include "renderer/Shader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>

namespace dw {

class PostProcess {
public:
    PostProcess() = default;
    ~PostProcess() = default;

    bool init(int width, int height);
    void beginCapture();
    void endAndRender(float time, const glm::mat4& projection);
    void onResize(int width, int height);
    void shutdown();

    // Distance fog parameters
    void setFogParams(const glm::vec3& color, float start, float end, float nearPlane, float farPlane);

    // SSAO toggle
    void setSSAOEnabled(bool enabled) { m_ssaoEnabled = enabled; }

    // SSAO debug view (F1) — shows raw AO buffer on screen
    void setSSAODebugView(bool enabled) { m_ssaoDebugView = enabled; }
    bool getSSAODebugView() const { return m_ssaoDebugView; }

private:
    void createFBO(int width, int height);
    void destroyFBO();
    void createBloomFBOs(int width, int height);
    void destroyBloomFBOs();
    void createSSAOResources(int width, int height);
    void destroySSAOResources();
    void generateSSAOKernel();
    void generateNoiseTexture();

    // Main post-process
    Shader m_shader;
    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    GLuint m_depthTex = 0;
    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;

    // Bloom
    Shader m_brightShader;
    Shader m_blurShader;
    GLuint m_brightFbo = 0;
    GLuint m_brightTex = 0;
    GLuint m_pingPongFbo[2] = {0, 0};
    GLuint m_pingPongTex[2] = {0, 0};
    int m_bloomWidth = 640;
    int m_bloomHeight = 360;

    static constexpr int BLUR_PASSES = 5;
    static constexpr float BLOOM_THRESHOLD = 0.8f;
    static constexpr float BLOOM_INTENSITY = 0.35f;

    // SSAO
    Shader m_ssaoShader;
    Shader m_ssaoBlurShader;
    GLuint m_ssaoFbo = 0;
    GLuint m_ssaoTex = 0;      // raw AO
    GLuint m_ssaoBlurFbo = 0;
    GLuint m_ssaoBlurTex = 0;  // blurred AO
    GLuint m_noiseTex = 0;
    std::vector<glm::vec3> m_ssaoKernel;
    bool m_ssaoEnabled = true;
    bool m_ssaoDebugView = false;
    int m_ssaoWidth = 640;
    int m_ssaoHeight = 360;

    static constexpr int SSAO_KERNEL_SIZE = 32;
    static constexpr float SSAO_RADIUS = 1.5f;
    static constexpr float SSAO_BIAS = 0.025f;

    // Fog
    glm::vec3 m_fogColor{0.02f, 0.02f, 0.04f};
    float m_fogStart = 15.0f;
    float m_fogEnd = 40.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
    bool m_fogEnabled = false;

    int m_width = 1280;
    int m_height = 720;
};

} // namespace dw
