#include "core/Engine.h"
#include "core/InputManager.h"
#include "renderer/Renderer.h"
#include "scene/Scene.h"
#include "audio/AudioManager.h"
#include "ui/UIOverlay.h"

#include <glad/gl.h>
#include <SDL.h>
#include <iostream>

namespace dw {

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::init(const std::string& title, int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_windowWidth, m_windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        return false;
    }

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (version == 0) {
        std::cerr << "Failed to load OpenGL with GLAD\n";
        return false;
    }

    std::cout << "Engine initialized\n";
    std::cout << "  OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "  GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "  GPU: " << glGetString(GL_RENDERER) << "\n";

    SDL_GL_SetSwapInterval(1);

    // Mouse capture is now set per-room (FP vs fixed-cam)

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glViewport(0, 0, m_windowWidth, m_windowHeight);

    // Input
    m_inputManager = std::make_unique<InputManager>();

    // Audio
    m_audio = std::make_unique<AudioManager>();
    if (!m_audio->init()) {
        std::cerr << "Audio init failed (continuing without audio)\n";
        m_audio.reset();
    } else {
        // Generate procedural footstep (soft stone-floor thud)
        m_audio->generateFootstepSound();
    }

    // Renderer
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->init(m_windowWidth, m_windowHeight)) {
        std::cerr << "Failed to init renderer\n";
        return false;
    }

    // UI Overlay
    m_ui = std::make_unique<UIOverlay>();
    if (!m_ui->init(m_windowWidth, m_windowHeight)) {
        std::cerr << "Failed to init UI overlay\n";
        return false;
    }

    // Scene
    m_scene = std::make_unique<Scene>();
    if (!m_scene->init(*m_renderer)) {
        std::cerr << "Failed to init scene\n";
        return false;
    }

    m_scene->setAudioManager(m_audio.get());
    m_scene->loadRoom("assets/rooms/hallway.json", *m_renderer);

    m_running = true;
    return true;
}

void Engine::run() {
    Uint64 lastTicks = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    while (m_running) {
        Uint64 currentTicks = SDL_GetPerformanceCounter();
        m_deltaTime = static_cast<float>(currentTicks - lastTicks) / static_cast<float>(freq);
        lastTicks = currentTicks;

        if (m_deltaTime > 0.1f) m_deltaTime = 0.1f;
        m_totalTime += m_deltaTime;

        m_inputManager->preUpdate();
        processEvents();
        update(m_deltaTime);
        render();

        SDL_GL_SwapWindow(m_window);
    }
}

void Engine::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_running = false;
                break;
            case SDL_KEYDOWN:
                m_inputManager->onKeyDown(event.key.keysym.scancode);
                if (event.key.keysym.sym == SDLK_ESCAPE) m_running = false;
                if (event.key.keysym.sym == SDLK_F1) {
                    auto& pp = m_renderer->getPostProcess();
                    pp.setSSAODebugView(!pp.getSSAODebugView());
                    std::cout << "SSAO debug view: " << (pp.getSSAODebugView() ? "ON" : "OFF") << "\n";
                }
                break;
            case SDL_KEYUP:
                m_inputManager->onKeyUp(event.key.keysym.scancode);
                break;
            case SDL_MOUSEMOTION:
                m_inputManager->onMouseMove(event.motion.xrel, event.motion.yrel);
                break;
            case SDL_MOUSEBUTTONDOWN:
                m_inputManager->onMouseDown(event.button.button);
                break;
            case SDL_MOUSEBUTTONUP:
                m_inputManager->onMouseUp(event.button.button);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    m_windowWidth = event.window.data1;
                    m_windowHeight = event.window.data2;
                    m_renderer->onResize(m_windowWidth, m_windowHeight);
                    if (m_ui) m_ui->onResize(m_windowWidth, m_windowHeight);
                }
                break;
        }
    }
}

void Engine::update(float dt) {
    m_scene->update(dt, *m_inputManager, *m_renderer, m_audio.get(), m_ui.get());
}

void Engine::render() {
    // 1. Shadow map pass (render from light's perspective)
    m_renderer->renderShadowPass(*m_scene, m_scene->getCurrentRoom().lightDir);

    // 2. Main scene (to PostProcess FBO)
    m_renderer->beginFrame();

    // 3. Pre-rendered background (depth write off)
    m_renderer->renderBackground();

    // 4. FMV overlays (animated atmospheric effects on background)
    m_scene->renderFMVOverlays();

    // 5. Depth pre-pass (hidden geometry for occlusion)
    m_renderer->renderDepthPrePass(*m_scene, m_scene->getCamera());

    // 5. Bind shadow map + render 3D objects
    m_scene->bindShadowUniforms(m_renderer->getShadowMap());
    m_scene->renderObjects();

    // 6. Particles (fog, dust, fireflies) — pass depth texture for soft particles
    {
        auto& pp = m_renderer->getPostProcess();
        m_scene->getParticleSystem().setDepthTexture(
            pp.getDepthTexture(), pp.getNearPlane(), pp.getFarPlane(),
            m_windowWidth, m_windowHeight);
    }
    m_scene->renderParticles();

    // 7. Post-processing (SSAO + bloom + fog) → screen
    m_renderer->endFrame(m_totalTime, m_scene->getCamera().getProjection());

    // 8. Overlays (room transition fade)
    m_scene->renderOverlays();

    // 9. UI (prompts, book viewer)
    if (m_ui) {
        m_scene->renderUI(*m_ui, m_windowWidth, m_windowHeight);
    }
}

void Engine::shutdown() {
    if (m_scene) m_scene->shutdown();
    m_scene.reset();
    if (m_ui) m_ui->shutdown();
    m_ui.reset();
    if (m_renderer) m_renderer->shutdown();
    m_renderer.reset();
    if (m_audio) m_audio->shutdown();
    m_audio.reset();
    m_inputManager.reset();

    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    std::cout << "Engine shutdown\n";
}

} // namespace dw
