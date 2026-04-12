#pragma once

#include <glad/gl.h>
#include <string>

namespace dw {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    bool load(const std::string& path);
    bool loadFromMemory(const unsigned char* data, int length);
    void bind(GLenum unit = GL_TEXTURE0) const;
    void destroy();

    GLuint getID() const { return m_id; }
    void setID(GLuint id) { destroy(); m_id = id; }  // for injecting procedurally generated textures
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace dw
