#pragma once

#include "renderer/Mesh.h"
#include "renderer/Shader.h"
#include "scene/Room.h"
#include <glm/glm.hpp>

namespace dw {

class InputManager;
class Camera;

class Player {
public:
    Player() = default;
    ~Player() = default;

    bool init();
    void update(float dt, const InputManager& input, const Camera& camera);
    void render(Shader& shader);

    const glm::vec3& getPosition() const { return m_position; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    float getRotation() const { return m_rotation; }
    float getPitch() const { return m_pitch; }

    // Smoothed values for camera
    float getSmoothYaw() const { return m_smoothYaw; }
    float getSmoothPitch() const { return m_smoothPitch; }

    // Head bob offset (vertical, only when moving)
    float getHeadBob() const { return m_headBobOffset; }

    // Forward look direction from yaw/pitch
    glm::vec3 getLookDirection() const;

    // Collision bounds (AABB half-extents)
    void setWorldBounds(float minX, float maxX, float minZ, float maxZ);
    void setCollisionBoxes(const std::vector<CollisionBox>& boxes);

private:
    bool collidesWithAny(float x, float z) const;
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
    float m_rotation = 0.0f; // Y-axis yaw in radians
    float m_pitch = 0.0f;    // vertical look angle in radians
    float m_moveSpeed = 7.0f;
    float m_rotSpeed = 3.0f;

    static constexpr float MOUSE_SENSITIVITY = 0.0015f;
    static constexpr float MAX_PITCH = 1.4835f; // ~85 degrees

    // Smooth mouse
    float m_smoothYaw = 0.0f;
    float m_smoothPitch = 0.0f;

    // Head bob
    float m_headBobTime = 0.0f;
    float m_headBobOffset = 0.0f;
    static constexpr float HEAD_BOB_FREQ = 8.0f;
    static constexpr float HEAD_BOB_AMP = 0.03f;

    // World bounds
    float m_boundsMinX = -10.0f;
    float m_boundsMaxX = 10.0f;
    float m_boundsMinZ = -10.0f;
    float m_boundsMaxZ = 10.0f;

    // Collision
    std::vector<CollisionBox> m_collisionBoxes;
    static constexpr float PLAYER_RADIUS = 0.3f;

    Mesh m_mesh; // simple placeholder mesh
};

} // namespace dw
