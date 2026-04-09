#pragma once

#include "scene/Room.h"
#include "scene/RoomTransition.h"
#include "renderer/Camera.h"
#include "renderer/Mesh.h"
#include "renderer/Texture.h"
#include "renderer/Shader.h"
#include "renderer/Model.h"
#include "fx/ParticleSystem.h"
#include "fx/FMVOverlay.h"
#include "world/Player.h"
#include "world/Book.h"

#include <string>
#include <memory>
#include <vector>

namespace dw {

class Renderer;
class InputManager;
class AudioManager;
class UIOverlay;
class ShadowMap;

struct TriggerZone {
    glm::vec3 position{0.0f};
    float radius = 1.0f;
    std::string targetRoom;
    glm::vec3 spawnPos{0.0f};
};

class Scene {
public:
    Scene();
    ~Scene();

    bool init(Renderer& renderer);
    bool loadRoom(const std::string& roomDefPath, Renderer& renderer);
    void update(float dt, const InputManager& input, Renderer& renderer, AudioManager* audio, UIOverlay* ui);
    void renderObjects();
    void renderFMVOverlays();
    void renderParticles();
    void renderOverlays();
    void renderUI(UIOverlay& ui, int screenWidth, int screenHeight);
    void shutdown();

    // Shadow/depth pass support
    void renderShadowCasters(Shader& shadowShader);
    void renderDepthGeometry(Shader& depthShader, Camera& camera);
    void bindShadowUniforms(ShadowMap& shadowMap);

    Camera& getCamera() { return m_camera; }
    Player& getPlayer() { return m_player; }
    ParticleSystem& getParticleSystem() { return m_particles; }
    const RoomDef& getCurrentRoom() const { return m_currentRoom; }
    void setAudioManager(AudioManager* audio) { m_audio = audio; }

private:
    RoomDef m_currentRoom;
    Camera m_camera;
    Shader m_meshShader;
    Player m_player;
    RoomTransition m_transition;
    ParticleSystem m_particles;
    std::vector<FMVOverlay> m_fmvOverlays;

    struct PropInstance {
        Mesh mesh;
        Texture texture;
        std::unique_ptr<Model> model;  // GLB model (used instead of mesh+texture when set)
        glm::mat4 transform{1.0f};
        float rotationSpeed = 0.0f;
        glm::vec3 materialColor{0.5f, 0.5f, 0.5f};
        int bookIndex = -1;  // if this prop represents a book, link to m_books index
    };
    std::vector<std::unique_ptr<PropInstance>> m_props;
    std::vector<TriggerZone> m_triggers;
    std::vector<PointLightDef> m_pointLights;

    // Depth pre-pass hidden geometry
    std::unique_ptr<Model> m_depthGeometry;

    std::vector<std::unique_ptr<Book>> m_books;
    int m_nearBookIndex = -1;  // index of book player is near, -1 if none
    Book* m_activeBook = nullptr;  // currently open book

    float m_propTime = 0.0f;
    bool m_playerMoving = false;
    float m_triggerCooldown = 0.0f;
    float m_highlightAmount = 0.0f;  // 0..1 smooth highlight for looked-at book

    AudioManager* m_audio = nullptr;  // non-owning, set during update

    // Procedural texture generation
    static GLuint generateProceduralArt(int seed, int width, int height);
};

} // namespace dw
