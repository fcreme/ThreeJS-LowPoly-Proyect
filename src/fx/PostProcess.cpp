#include "fx/PostProcess.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
#include <cmath>

namespace dw {

bool PostProcess::init(int width, int height) {
    m_width = width;
    m_height = height;
    m_bloomWidth = width / 2;
    m_bloomHeight = height / 2;
    m_ssaoWidth = width / 2;
    m_ssaoHeight = height / 2;

    if (!m_shader.loadFromFile("assets/shaders/postprocess.vert", "assets/shaders/postprocess.frag")) {
        std::cerr << "PostProcess: failed to load shaders\n";
        return false;
    }

    if (!m_brightShader.loadFromFile("assets/shaders/postprocess.vert", "assets/shaders/bloom_bright.frag")) {
        std::cerr << "PostProcess: failed to load bloom brightness shader\n";
        return false;
    }

    if (!m_blurShader.loadFromFile("assets/shaders/postprocess.vert", "assets/shaders/bloom_blur.frag")) {
        std::cerr << "PostProcess: failed to load bloom blur shader\n";
        return false;
    }

    if (!m_ssaoShader.loadFromFile("assets/shaders/postprocess.vert", "assets/shaders/ssao.frag")) {
        std::cerr << "PostProcess: failed to load SSAO shader (disabling SSAO)\n";
        m_ssaoEnabled = false;
    }

    if (!m_ssaoBlurShader.loadFromFile("assets/shaders/postprocess.vert", "assets/shaders/ssao_blur.frag")) {
        std::cerr << "PostProcess: failed to load SSAO blur shader (disabling SSAO)\n";
        m_ssaoEnabled = false;
    }

    // Fullscreen quad
    float quad[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);
    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    createFBO(width, height);
    createBloomFBOs(m_bloomWidth, m_bloomHeight);

    generateSSAOKernel();
    generateNoiseTexture();
    createSSAOResources(m_ssaoWidth, m_ssaoHeight);

    std::cout << "PostProcess initialized (HDR bloom + SSAO + distance fog)\n";
    return true;
}

void PostProcess::generateSSAOKernel() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::mt19937 gen(42);

    m_ssaoKernel.resize(SSAO_KERNEL_SIZE);
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        glm::vec3 sample(
            dist(gen) * 2.0f - 1.0f,
            dist(gen) * 2.0f - 1.0f,
            dist(gen)  // hemisphere: z always positive
        );
        sample = glm::normalize(sample) * dist(gen);

        // Accelerating interpolation — cluster samples closer to origin
        float scale = static_cast<float>(i) / static_cast<float>(SSAO_KERNEL_SIZE);
        scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale*scale)
        sample *= scale;
        m_ssaoKernel[i] = sample;
    }
}

void PostProcess::generateNoiseTexture() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::mt19937 gen(123);

    // 4x4 noise texture (tiled across screen)
    // Store values already in [-1,1] range for XY, 0 for Z
    // so that the shader's *2-1 remap produces correct tangent-space rotations
    std::vector<float> noise(4 * 4 * 3);
    for (int i = 0; i < 16; i++) {
        noise[i * 3 + 0] = dist(gen) * 2.0f - 1.0f;
        noise[i * 3 + 1] = dist(gen) * 2.0f - 1.0f;
        noise[i * 3 + 2] = 0.0f; // rotation around z only
    }

    glGenTextures(1, &m_noiseTex);
    glBindTexture(GL_TEXTURE_2D, m_noiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void PostProcess::createFBO(int width, int height) {
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // HDR color attachment
    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    // Depth as sampleable texture for fog + SSAO
    glGenTextures(1, &m_depthTex);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PostProcess: framebuffer incomplete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::createSSAOResources(int width, int height) {
    // SSAO FBO (single-channel R16F)
    glGenFramebuffers(1, &m_ssaoFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFbo);

    glGenTextures(1, &m_ssaoTex);
    glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PostProcess: SSAO FBO incomplete!\n";
    }

    // SSAO blur FBO
    glGenFramebuffers(1, &m_ssaoBlurFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFbo);

    glGenTextures(1, &m_ssaoBlurTex);
    glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoBlurTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PostProcess: SSAO blur FBO incomplete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::createBloomFBOs(int width, int height) {
    glGenFramebuffers(1, &m_brightFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_brightFbo);

    glGenTextures(1, &m_brightTex);
    glBindTexture(GL_TEXTURE_2D, m_brightTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brightTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PostProcess: bloom bright FBO incomplete!\n";
    }

    glGenFramebuffers(2, m_pingPongFbo);
    glGenTextures(2, m_pingPongTex);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFbo[i]);
        glBindTexture(GL_TEXTURE_2D, m_pingPongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingPongTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "PostProcess: bloom ping-pong FBO " << i << " incomplete!\n";
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::destroyFBO() {
    if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
    if (m_depthTex) { glDeleteTextures(1, &m_depthTex); m_depthTex = 0; }
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
}

void PostProcess::destroyBloomFBOs() {
    if (m_brightTex) { glDeleteTextures(1, &m_brightTex); m_brightTex = 0; }
    if (m_brightFbo) { glDeleteFramebuffers(1, &m_brightFbo); m_brightFbo = 0; }
    glDeleteTextures(2, m_pingPongTex);
    m_pingPongTex[0] = 0; m_pingPongTex[1] = 0;
    glDeleteFramebuffers(2, m_pingPongFbo);
    m_pingPongFbo[0] = 0; m_pingPongFbo[1] = 0;
}

void PostProcess::destroySSAOResources() {
    if (m_ssaoTex) { glDeleteTextures(1, &m_ssaoTex); m_ssaoTex = 0; }
    if (m_ssaoFbo) { glDeleteFramebuffers(1, &m_ssaoFbo); m_ssaoFbo = 0; }
    if (m_ssaoBlurTex) { glDeleteTextures(1, &m_ssaoBlurTex); m_ssaoBlurTex = 0; }
    if (m_ssaoBlurFbo) { glDeleteFramebuffers(1, &m_ssaoBlurFbo); m_ssaoBlurFbo = 0; }
    if (m_noiseTex) { glDeleteTextures(1, &m_noiseTex); m_noiseTex = 0; }
}

void PostProcess::beginCapture() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcess::endAndRender(float time, const glm::mat4& projection) {
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_quadVao);

    // --- SSAO pass (at half res) ---
    if (m_ssaoEnabled && m_ssaoShader.getProgram() != 0) {
        // Step A: Compute raw SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFbo);
        glViewport(0, 0, m_ssaoWidth, m_ssaoHeight);
        glClear(GL_COLOR_BUFFER_BIT);

        m_ssaoShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_depthTex);
        m_ssaoShader.setInt("uDepthTex", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_noiseTex);
        m_ssaoShader.setInt("uNoiseTex", 1);

        // Upload kernel samples
        for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
            std::string name = "uSamples[" + std::to_string(i) + "]";
            m_ssaoShader.setVec3(name, m_ssaoKernel[i].x, m_ssaoKernel[i].y, m_ssaoKernel[i].z);
        }

        m_ssaoShader.setMat4("uProjection", glm::value_ptr(projection));
        glm::mat4 invProj = glm::inverse(projection);
        m_ssaoShader.setMat4("uInvProjection", glm::value_ptr(invProj));
        m_ssaoShader.setVec2("uNoiseScale",
            static_cast<float>(m_ssaoWidth) / 4.0f,
            static_cast<float>(m_ssaoHeight) / 4.0f);
        m_ssaoShader.setFloat("uRadius", SSAO_RADIUS);
        m_ssaoShader.setFloat("uBias", SSAO_BIAS);
        m_ssaoShader.setFloat("uNearPlane", m_nearPlane);
        m_ssaoShader.setFloat("uFarPlane", m_farPlane);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Step B: Blur SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFbo);
        glClear(GL_COLOR_BUFFER_BIT);

        m_ssaoBlurShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
        m_ssaoBlurShader.setInt("uSSAO", 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // --- SSAO debug view: render raw AO buffer to screen ---
    if (m_ssaoDebugView && m_ssaoEnabled && m_ssaoShader.getProgram() != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_width, m_height);
        glClear(GL_COLOR_BUFFER_BIT);

        // Re-use the postprocess shader in a minimal way — bind AO as scene color
        m_shader.use();
        // Bind blurred SSAO as the scene texture (it's single-channel, will show as grayscale)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
        m_shader.setInt("uScreen", 0);

        // Disable bloom, SSAO multiply, and fog for debug
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex); // dummy
        m_shader.setInt("uBloomTex", 1);
        m_shader.setFloat("uBloomIntensity", 0.0f);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_depthTex);
        m_shader.setInt("uDepthTex", 2);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
        m_shader.setInt("uSSAOTex", 3);
        m_shader.setInt("uSSAOEnabled", 0); // Don't multiply again
        m_shader.setInt("uFogEnabled", 0);
        m_shader.setFloat("uTime", time);
        m_shader.setVec2("uResolution", static_cast<float>(m_width), static_cast<float>(m_height));
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        return; // Skip normal composite
    }

    // --- Bloom: brightness extraction ---
    glBindFramebuffer(GL_FRAMEBUFFER, m_brightFbo);
    glViewport(0, 0, m_bloomWidth, m_bloomHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    m_brightShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    m_brightShader.setInt("uScreen", 0);
    m_brightShader.setFloat("uThreshold", BLOOM_THRESHOLD);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- Bloom: ping-pong blur ---
    bool horizontal = true;
    bool firstPass = true;
    m_blurShader.use();

    for (int i = 0; i < BLUR_PASSES * 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFbo[horizontal ? 1 : 0]);
        glClear(GL_COLOR_BUFFER_BIT);

        m_blurShader.setInt("uImage", 0);
        m_blurShader.setInt("uHorizontal", horizontal ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, firstPass ? m_brightTex : m_pingPongTex[horizontal ? 0 : 1]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        horizontal = !horizontal;
        firstPass = false;
    }

    // --- Final composite to screen ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    m_shader.setInt("uScreen", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_pingPongTex[0]);
    m_shader.setInt("uBloomTex", 1);
    m_shader.setFloat("uBloomIntensity", BLOOM_INTENSITY);

    // Depth texture for fog
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);
    m_shader.setInt("uDepthTex", 2);

    // SSAO texture
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
    m_shader.setInt("uSSAOTex", 3);
    m_shader.setInt("uSSAOEnabled", (m_ssaoEnabled && m_ssaoShader.getProgram() != 0) ? 1 : 0);

    // Fog uniforms
    m_shader.setInt("uFogEnabled", m_fogEnabled ? 1 : 0);
    m_shader.setVec3("uFogColor", m_fogColor.r, m_fogColor.g, m_fogColor.b);
    m_shader.setFloat("uFogStart", m_fogStart);
    m_shader.setFloat("uFogEnd", m_fogEnd);
    m_shader.setFloat("uNearPlane", m_nearPlane);
    m_shader.setFloat("uFarPlane", m_farPlane);

    m_shader.setFloat("uTime", time);
    m_shader.setVec2("uResolution", static_cast<float>(m_width), static_cast<float>(m_height));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void PostProcess::setFogParams(const glm::vec3& color, float start, float end, float nearPlane, float farPlane) {
    m_fogColor = color;
    m_fogStart = start;
    m_fogEnd = end;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    m_fogEnabled = true;
}

void PostProcess::onResize(int width, int height) {
    m_width = width;
    m_height = height;
    m_bloomWidth = width / 2;
    m_bloomHeight = height / 2;
    m_ssaoWidth = width / 2;
    m_ssaoHeight = height / 2;
    destroyFBO();
    destroyBloomFBOs();
    destroySSAOResources();
    createFBO(width, height);
    createBloomFBOs(m_bloomWidth, m_bloomHeight);
    generateNoiseTexture();
    createSSAOResources(m_ssaoWidth, m_ssaoHeight);
}

void PostProcess::shutdown() {
    destroySSAOResources();
    destroyBloomFBOs();
    destroyFBO();
    if (m_quadVbo) { glDeleteBuffers(1, &m_quadVbo); m_quadVbo = 0; }
    if (m_quadVao) { glDeleteVertexArrays(1, &m_quadVao); m_quadVao = 0; }
}

} // namespace dw
