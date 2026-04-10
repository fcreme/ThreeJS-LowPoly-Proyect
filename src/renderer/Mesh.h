#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace dw {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec3 tangent{0.0f, 1.0f, 0.0f};
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void draw() const;
    void destroy();

    // Simple primitive generators
    static Mesh createCube(float size = 1.0f);
    static Mesh createPlane(float width, float depth);

    bool loadOBJ(const std::string& path);

private:
    void setupBuffers(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    uint32_t m_indexCount = 0;
};

} // namespace dw
