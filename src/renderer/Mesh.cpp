#include "renderer/Mesh.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace dw {

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo), m_indexCount(other.m_indexCount) {
    other.m_vao = other.m_vbo = other.m_ebo = 0;
    other.m_indexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        destroy();
        m_vao = other.m_vao; m_vbo = other.m_vbo; m_ebo = other.m_ebo;
        m_indexCount = other.m_indexCount;
        other.m_vao = other.m_vbo = other.m_ebo = 0;
        other.m_indexCount = 0;
    }
    return *this;
}

void Mesh::upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    destroy();
    setupBuffers(vertices, indices);
}

void Mesh::draw() const {
    if (m_vao == 0) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::destroy() {
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    m_indexCount = 0;
}

void Mesh::setupBuffers(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    m_indexCount = static_cast<uint32_t>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // Texcoord (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));

    // Tangent (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

    glBindVertexArray(0);
}

Mesh Mesh::createCube(float size) {
    float h = size * 0.5f;
    std::vector<Vertex> verts = {
        // Front  (normal +Z, tangent +X)
        {{-h, -h,  h}, {0,0,1}, {0,0}, {1,0,0}}, {{ h, -h,  h}, {0,0,1}, {1,0}, {1,0,0}},
        {{ h,  h,  h}, {0,0,1}, {1,1}, {1,0,0}}, {{-h,  h,  h}, {0,0,1}, {0,1}, {1,0,0}},
        // Back   (normal -Z, tangent -X)
        {{ h, -h, -h}, {0,0,-1}, {0,0}, {-1,0,0}}, {{-h, -h, -h}, {0,0,-1}, {1,0}, {-1,0,0}},
        {{-h,  h, -h}, {0,0,-1}, {1,1}, {-1,0,0}}, {{ h,  h, -h}, {0,0,-1}, {0,1}, {-1,0,0}},
        // Top    (normal +Y, tangent +X)
        {{-h,  h,  h}, {0,1,0}, {0,0}, {1,0,0}}, {{ h,  h,  h}, {0,1,0}, {1,0}, {1,0,0}},
        {{ h,  h, -h}, {0,1,0}, {1,1}, {1,0,0}}, {{-h,  h, -h}, {0,1,0}, {0,1}, {1,0,0}},
        // Bottom (normal -Y, tangent +X)
        {{-h, -h, -h}, {0,-1,0}, {0,0}, {1,0,0}}, {{ h, -h, -h}, {0,-1,0}, {1,0}, {1,0,0}},
        {{ h, -h,  h}, {0,-1,0}, {1,1}, {1,0,0}}, {{-h, -h,  h}, {0,-1,0}, {0,1}, {1,0,0}},
        // Right  (normal +X, tangent +Z)
        {{ h, -h,  h}, {1,0,0}, {0,0}, {0,0,-1}}, {{ h, -h, -h}, {1,0,0}, {1,0}, {0,0,-1}},
        {{ h,  h, -h}, {1,0,0}, {1,1}, {0,0,-1}}, {{ h,  h,  h}, {1,0,0}, {0,1}, {0,0,-1}},
        // Left   (normal -X, tangent +Z)
        {{-h, -h, -h}, {-1,0,0}, {0,0}, {0,0,1}}, {{-h, -h,  h}, {-1,0,0}, {1,0}, {0,0,1}},
        {{-h,  h,  h}, {-1,0,0}, {1,1}, {0,0,1}}, {{-h,  h, -h}, {-1,0,0}, {0,1}, {0,0,1}},
    };
    std::vector<uint32_t> idx;
    for (uint32_t face = 0; face < 6; face++) {
        uint32_t base = face * 4;
        idx.insert(idx.end(), {base, base+1, base+2, base, base+2, base+3});
    }
    Mesh m;
    m.upload(verts, idx);
    return m;
}

Mesh Mesh::createPlane(float width, float depth) {
    float hw = width * 0.5f, hd = depth * 0.5f;
    std::vector<Vertex> verts = {
        {{-hw, 0, -hd}, {0,1,0}, {0,0}, {1,0,0}}, {{ hw, 0, -hd}, {0,1,0}, {1,0}, {1,0,0}},
        {{ hw, 0,  hd}, {0,1,0}, {1,1}, {1,0,0}}, {{-hw, 0,  hd}, {0,1,0}, {0,1}, {1,0,0}},
    };
    std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};
    Mesh m;
    m.upload(verts, idx);
    return m;
}

bool Mesh::loadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Mesh: cannot open OBJ: " << path << "\n";
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 p;
            iss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (prefix == "vn") {
            glm::vec3 n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (prefix == "vt") {
            glm::vec2 t;
            iss >> t.x >> t.y;
            texcoords.push_back(t);
        } else if (prefix == "f") {
            // Parse face — supports v, v/vt, v/vt/vn, v//vn
            std::string vertStr;
            std::vector<uint32_t> faceIndices;
            while (iss >> vertStr) {
                int vi = 0, ti = 0, ni = 0;
                if (sscanf(vertStr.c_str(), "%d/%d/%d", &vi, &ti, &ni) == 3) {
                } else if (sscanf(vertStr.c_str(), "%d//%d", &vi, &ni) == 2) {
                } else if (sscanf(vertStr.c_str(), "%d/%d", &vi, &ti) == 2) {
                } else {
                    sscanf(vertStr.c_str(), "%d", &vi);
                }

                Vertex v{};
                if (vi > 0 && vi <= (int)positions.size()) v.position = positions[vi - 1];
                if (ni > 0 && ni <= (int)normals.size())   v.normal = normals[ni - 1];
                if (ti > 0 && ti <= (int)texcoords.size())  v.texcoord = texcoords[ti - 1];

                uint32_t idx = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
                faceIndices.push_back(idx);
            }
            // Triangulate (fan)
            for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                indices.push_back(faceIndices[0]);
                indices.push_back(faceIndices[i]);
                indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    if (vertices.empty()) {
        std::cerr << "Mesh: OBJ has no vertices: " << path << "\n";
        return false;
    }

    upload(vertices, indices);
    std::cout << "Mesh: loaded " << path << " (" << vertices.size() << " verts, "
              << indices.size() / 3 << " tris)\n";
    return true;
}

} // namespace dw
