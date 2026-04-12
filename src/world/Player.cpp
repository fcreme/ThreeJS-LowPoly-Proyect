#include "world/Player.h"
#include "core/InputManager.h"
#include "renderer/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace dw {

bool Player::init() {
    // Simple capsule-like shape: a tall cube as placeholder
    m_mesh = Mesh::createCube(1.0f);
    std::cout << "Player initialized\n";
    return true;
}

void Player::update(float dt, const InputManager& input, const Camera& camera) {
    (void)camera;

    // Mouse look (negate X so moving mouse right turns camera right)
    m_rotation -= input.getMouseDeltaX() * MOUSE_SENSITIVITY;
    m_pitch -= input.getMouseDeltaY() * MOUSE_SENSITIVITY;
    m_pitch = std::clamp(m_pitch, -MAX_PITCH, MAX_PITCH);

    // Smooth mouse interpolation
    float lerpFactor = 1.0f - std::pow(0.001f, dt * 10.0f); // ~0.3 per frame at 60fps
    m_smoothYaw += (m_rotation - m_smoothYaw) * lerpFactor;
    m_smoothPitch += (m_pitch - m_smoothPitch) * lerpFactor;

    // FPS movement: forward/back and strafe relative to yaw direction
    float forwardX = std::sin(m_rotation);
    float forwardZ = std::cos(m_rotation);
    float rightX = forwardZ;   // perpendicular to forward on XZ plane
    float rightZ = -forwardX;

    glm::vec3 movement(0.0f);
    if (input.isKeyDown(SDL_SCANCODE_W)) {
        movement.x += forwardX * m_moveSpeed * dt;
        movement.z += forwardZ * m_moveSpeed * dt;
    }
    if (input.isKeyDown(SDL_SCANCODE_S)) {
        movement.x -= forwardX * m_moveSpeed * dt;
        movement.z -= forwardZ * m_moveSpeed * dt;
    }
    if (input.isKeyDown(SDL_SCANCODE_A)) {
        movement.x += rightX * m_moveSpeed * dt;
        movement.z += rightZ * m_moveSpeed * dt;
    }
    if (input.isKeyDown(SDL_SCANCODE_D)) {
        movement.x -= rightX * m_moveSpeed * dt;
        movement.z -= rightZ * m_moveSpeed * dt;
    }

    // Try X and Z movement separately for slide-along-wall collision
    float newX = m_position.x + movement.x;
    float newZ = m_position.z + movement.z;

    // Test X axis
    if (!collidesWithAny(newX, m_position.z)) {
        m_position.x = newX;
    }
    // Test Z axis
    if (!collidesWithAny(m_position.x, newZ)) {
        m_position.z = newZ;
    }

    // Clamp to world bounds
    m_position.x = std::clamp(m_position.x, m_boundsMinX, m_boundsMaxX);
    m_position.z = std::clamp(m_position.z, m_boundsMinZ, m_boundsMaxZ);

    // Head bob — scale frequency by actual movement speed for consistency
    float moveLen = glm::length(movement);
    bool isMoving = moveLen > 0.001f;
    if (isMoving) {
        float speedRatio = (moveLen / dt) / m_moveSpeed; // normalized to max speed
        m_headBobTime += dt * std::min(speedRatio, 1.0f);
        m_headBobOffset = std::sin(m_headBobTime * HEAD_BOB_FREQ) * HEAD_BOB_AMP;
    } else {
        // Smoothly return to zero
        m_headBobOffset *= 0.9f;
        if (std::abs(m_headBobOffset) < 0.001f) m_headBobOffset = 0.0f;
    }
}

glm::vec3 Player::getLookDirection() const {
    glm::vec3 front;
    front.x = std::cos(m_smoothPitch) * std::sin(m_smoothYaw);
    front.y = std::sin(m_smoothPitch);
    front.z = std::cos(m_smoothPitch) * std::cos(m_smoothYaw);
    return glm::normalize(front);
}

void Player::render(Shader& shader) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, m_position + glm::vec3(0.0f, 0.5f, 0.0f)); // raise to stand on ground
    model = glm::rotate(model, m_rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(0.4f, 0.9f, 0.3f)); // tall thin box

    shader.setMat4("uModel", glm::value_ptr(model));
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    shader.setMat3("uNormalMatrix", glm::value_ptr(normalMatrix));
    m_mesh.draw();
}

void Player::setWorldBounds(float minX, float maxX, float minZ, float maxZ) {
    m_boundsMinX = minX;
    m_boundsMaxX = maxX;
    m_boundsMinZ = minZ;
    m_boundsMaxZ = maxZ;
}

void Player::setCollisionBoxes(const std::vector<CollisionBox>& boxes) {
    m_collisionBoxes = boxes;
}

bool Player::collidesWithAny(float x, float z) const {
    // Player is a circle with PLAYER_RADIUS on the XZ plane
    // Test against each AABB using closest-point distance
    for (const auto& box : m_collisionBoxes) {
        float closestX = std::clamp(x, box.min.x, box.max.x);
        float closestZ = std::clamp(z, box.min.y, box.max.y);
        float dx = x - closestX;
        float dz = z - closestZ;
        if (dx * dx + dz * dz < PLAYER_RADIUS * PLAYER_RADIUS) {
            return true;
        }
    }
    return false;
}

} // namespace dw
