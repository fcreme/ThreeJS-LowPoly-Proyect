#include "scene/Scene.h"
#include "renderer/Renderer.h"
#include "renderer/BackgroundLayer.h"
#include "renderer/ShadowMap.h"
#include "core/InputManager.h"
#include "audio/AudioManager.h"
#include "ui/UIOverlay.h"

#include <SDL.h>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

namespace dw {

namespace {
ParticleType parseParticleType(const std::string& s) {
    if (s == "fog") return ParticleType::Fog;
    if (s == "fireflies") return ParticleType::Fireflies;
    if (s == "fire") return ParticleType::Fire;
    return ParticleType::Dust;
}
} // anonymous namespace

Scene::Scene() = default;
Scene::~Scene() = default;

bool Scene::init(Renderer& renderer) {
    (void)renderer;

    if (!m_meshShader.loadFromFile("assets/shaders/mesh.vert", "assets/shaders/mesh.frag")) {
        std::cerr << "Scene: failed to load mesh shader\n";
        return false;
    }

    if (!m_player.init()) {
        std::cerr << "Scene: failed to init player\n";
        return false;
    }

    if (!m_transition.init()) {
        std::cerr << "Scene: failed to init room transition\n";
        return false;
    }

    if (!m_particles.init()) {
        std::cerr << "Scene: failed to init particle system\n";
        return false;
    }

    std::cout << "Scene initialized\n";
    return true;
}

bool Scene::loadRoom(const std::string& roomDefPath, Renderer& renderer) {
    m_props.clear();
    m_triggers.clear();
    m_pointLights.clear();
    m_particles.clear();
    m_fmvOverlays.clear();
    m_books.clear();
    m_nearBookIndex = -1;
    m_activeBook = nullptr;
    m_depthGeometry.reset();

    if (!loadRoomDef(roomDefPath, m_currentRoom)) {
        return false;
    }

    // Camera
    float aspect = static_cast<float>(renderer.getWidth()) / static_cast<float>(renderer.getHeight());
    m_camera.setup(m_currentRoom.cameraPos, m_currentRoom.cameraTarget);
    m_camera.setPerspective(m_currentRoom.cameraFov, aspect, 0.1f, 100.0f);

    // Background
    if (!m_currentRoom.backgroundPath.empty()) {
        renderer.getBackgroundLayer().loadTexture(m_currentRoom.backgroundPath);
    }

    // Load props (any format via Assimp)
    for (const auto& propDef : m_currentRoom.props) {
        auto prop = std::make_unique<PropInstance>();

        if (!propDef.modelPath.empty()) {
            prop->model = std::make_unique<Model>();
            prop->model->load(propDef.modelPath);
        }
        if (!propDef.texturePath.empty()) prop->texture.load(propDef.texturePath);

        glm::mat4 t(1.0f);
        t = glm::translate(t, propDef.position);
        t = glm::rotate(t, glm::radians(propDef.rotation.y), glm::vec3(0, 1, 0));
        t = glm::rotate(t, glm::radians(propDef.rotation.x), glm::vec3(1, 0, 0));
        t = glm::rotate(t, glm::radians(propDef.rotation.z), glm::vec3(0, 0, 1));
        t = glm::scale(t, propDef.scale);
        prop->transform = t;
        m_props.push_back(std::move(prop));
    }

    // === Room-specific procedural content ===
    if (m_currentRoom.name == "Forest Clearing") {
        // Stone pillars (dark gray)
        auto addPillar = [&](float x, float z) {
            auto p = std::make_unique<PropInstance>();
            p->mesh = Mesh::createCube(1.0f);
            p->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.75f, z)), glm::vec3(0.5f, 1.5f, 0.5f));
            p->materialColor = glm::vec3(0.25f, 0.23f, 0.22f);
            p->roughness = 0.9f; // rough stone
            m_props.push_back(std::move(p));
        };
        addPillar(-2.0f, -1.0f);
        addPillar(2.0f, -1.0f);

        // Floating book (animated)
        auto book = std::make_unique<PropInstance>();
        book->mesh = Mesh::createCube(1.0f);
        book->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, -2.0f)), glm::vec3(0.6f, 0.05f, 0.4f));
        book->rotationSpeed = 0.8f;
        book->materialColor = glm::vec3(0.4f, 0.2f, 0.1f); // leather brown
        book->roughness = 0.5f; // semi-smooth leather
        m_props.push_back(std::move(book));

        // Ground plane
        auto ground = std::make_unique<PropInstance>();
        ground->mesh = Mesh::createPlane(12.0f, 12.0f);
        ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        ground->materialColor = glm::vec3(0.08f, 0.1f, 0.06f); // dark earth
        ground->roughness = 0.85f; // rough earth
        m_props.push_back(std::move(ground));

        // Point lights (lanterns near pillars)
        m_pointLights.push_back({{-2.0f, 2.0f, -1.0f}, {0.9f, 0.6f, 0.2f}, 6.0f, 6.0f, true});
        m_pointLights.push_back({{2.0f, 2.0f, -1.0f}, {0.9f, 0.6f, 0.2f}, 6.0f, 6.0f, true});

        // Trigger
        TriggerZone door;
        door.position = glm::vec3(0.0f, 0.0f, -4.0f);
        door.radius = 1.5f;
        door.targetRoom = "assets/rooms/hallway.json";
        door.spawnPos = glm::vec3(0.0f, 0.0f, 3.0f);
        m_triggers.push_back(door);

        // Interactive book at the floating book position
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(0.0f, 1.2f, -2.0f), 2.5f);
            b->addPage("assets/textures/book_page1.png");
            b->addPage("assets/textures/book_page2.png");
            b->addPage("assets/textures/book_page3.png");
            m_books.push_back(std::move(b));
        }

        m_player.setWorldBounds(-4.0f, 4.0f, -4.5f, 4.0f);

        // Fog params for outdoor forest (lighter, farther)
        // Exponential fog: uFogStart=density, uFogEnd=max distance
        renderer.getPostProcess().setFogParams(
            glm::vec3(0.03f, 0.04f, 0.05f), 3.0f, 50.0f, 0.1f, 100.0f);
    }
    else if (m_currentRoom.name == "Dark Hallway") {
        // Picture Gallery GLB model — scaled up so player is inside the room
        {
            auto prop = std::make_unique<PropInstance>();
            prop->model = std::make_unique<Model>();
            if (prop->model->load("props/the_picture_gallery_low_poly__vr.glb")) {
                glm::mat4 t(1.0f);
                t = glm::translate(t, glm::vec3(0.0f, 0.0f, 0.0f));
                t = glm::scale(t, glm::vec3(10.0f));
                prop->transform = t;
                m_props.push_back(std::move(prop));
            }
        }

        // Lantern positions (used for props, lights, and fire particles)
        struct LanternPos { float x, y, z; };
        LanternPos lanterns[] = {
            {-8.0f, 5.0f, 4.0f}, {8.0f, 5.0f, 4.0f},
            {-8.0f, 5.0f, -8.0f}, {8.0f, 5.0f, -8.0f},
            {0.0f, 5.0f, -14.0f}, {0.0f, 5.0f, 10.0f}
        };

        for (const auto& lp : lanterns) {
            // Bronze lantern cube
            auto l = std::make_unique<PropInstance>();
            l->mesh = Mesh::createCube(1.0f);
            l->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(lp.x, lp.y, lp.z)), glm::vec3(0.3f, 0.5f, 0.3f));
            l->materialColor = glm::vec3(0.6f, 0.4f, 0.1f);
            l->roughness = 0.2f; // polished bronze
            l->emissive = glm::vec3(0.3f, 0.2f, 0.05f); // subtle warm self-illumination
            m_props.push_back(std::move(l));
        }

        // Table
        auto table = std::make_unique<PropInstance>();
        table->mesh = Mesh::createCube(1.0f);
        table->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)), glm::vec3(1.5f, 0.06f, 0.8f));
        table->materialColor = glm::vec3(0.3f, 0.18f, 0.08f);
        table->roughness = 0.7f; // wood
        m_props.push_back(std::move(table));

        // Pedestals for books
        auto addPedestal = [&](float x, float z) {
            auto p = std::make_unique<PropInstance>();
            p->mesh = Mesh::createCube(1.0f);
            p->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.4f, z)), glm::vec3(0.4f, 0.8f, 0.4f));
            p->materialColor = glm::vec3(0.2f, 0.15f, 0.1f);
            p->roughness = 0.7f; // wood
            m_props.push_back(std::move(p));
        };
        addPedestal(-5.0f, -4.0f);
        addPedestal(5.0f, -4.0f);
        addPedestal(-5.0f, 8.0f);
        addPedestal(5.0f, 8.0f);

        // Picture frames with procedural art textures
        int frameSeed = 100;
        auto addFrame = [&](float x, float y, float z, float ry, float w, float h) {
            auto f = std::make_unique<PropInstance>();
            f->mesh = Mesh::createCube(1.0f);
            glm::mat4 t(1.0f);
            t = glm::translate(t, glm::vec3(x, y, z));
            t = glm::rotate(t, glm::radians(ry), glm::vec3(0, 1, 0));
            t = glm::scale(t, glm::vec3(w, h, 0.05f));
            f->transform = t;
            f->materialColor = glm::vec3(0.35f, 0.25f, 0.15f);
            f->roughness = 0.6f; // varnished wood frame
            // Generate procedural artwork texture
            GLuint artTex = generateProceduralArt(frameSeed++, 256, 256);
            f->texture = Texture(); // default constructed
            f->texture.setID(artTex); // inject generated texture
            m_props.push_back(std::move(f));
        };
        // Left wall
        addFrame(-9.5f, 3.5f, -2.0f, 90.0f, 2.0f, 1.5f);
        addFrame(-9.5f, 3.5f, 6.0f, 90.0f, 1.8f, 2.0f);
        // Right wall
        addFrame(9.5f, 3.5f, -2.0f, -90.0f, 2.0f, 1.5f);
        addFrame(9.5f, 3.5f, 6.0f, -90.0f, 1.8f, 2.0f);
        // Back wall
        addFrame(-3.0f, 4.0f, -15.5f, 0.0f, 2.5f, 1.8f);
        addFrame(3.0f, 4.0f, -15.5f, 0.0f, 2.0f, 2.5f);

        // Rope barriers
        auto addRope = [&](float x1, float z1, float x2, float z2, float y) {
            float cx = (x1 + x2) * 0.5f;
            float cz = (z1 + z2) * 0.5f;
            float dx = x2 - x1, dz = z2 - z1;
            float len = std::sqrt(dx * dx + dz * dz);
            float angle = std::atan2(dx, dz);
            auto r = std::make_unique<PropInstance>();
            r->mesh = Mesh::createCube(1.0f);
            glm::mat4 t(1.0f);
            t = glm::translate(t, glm::vec3(cx, y, cz));
            t = glm::rotate(t, angle, glm::vec3(0, 1, 0));
            t = glm::scale(t, glm::vec3(0.03f, 0.03f, len));
            r->transform = t;
            r->materialColor = glm::vec3(0.5f, 0.1f, 0.1f);
            r->roughness = 0.8f; // rope/fabric
            m_props.push_back(std::move(r));
        };
        addRope(-5.0f, -4.0f, 5.0f, -4.0f, 0.85f);
        addRope(-5.0f, 8.0f, 5.0f, 8.0f, 0.85f);

        // Warm lantern lights
        m_pointLights.push_back({{-8.0f, 6.0f, 4.0f}, {1.0f, 0.7f, 0.3f}, 25.0f, 25.0f, true});
        m_pointLights.push_back({{8.0f, 6.0f, 4.0f}, {1.0f, 0.7f, 0.3f}, 25.0f, 25.0f, true});
        m_pointLights.push_back({{-8.0f, 6.0f, -8.0f}, {0.8f, 0.5f, 0.2f}, 20.0f, 20.0f, true});
        m_pointLights.push_back({{8.0f, 6.0f, -8.0f}, {0.8f, 0.5f, 0.2f}, 20.0f, 20.0f, true});

        // Triggers disabled — testing hallway only

        // Books
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(0.0f, 0.6f, 0.0f), 3.0f);
            b->addPage("assets/textures/book_page1.png");
            b->addPage("assets/textures/book_page2.png");
            m_books.push_back(std::move(b));
        }
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(-5.0f, 0.9f, -4.0f), 3.0f);
            b->addPage("assets/textures/book_page1.png");
            m_books.push_back(std::move(b));
        }
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(5.0f, 0.9f, -4.0f), 3.0f);
            b->addPage("assets/textures/book_page2.png");
            m_books.push_back(std::move(b));
        }
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(-5.0f, 0.9f, 8.0f), 3.0f);
            b->addPage("assets/textures/book_page3.png");
            m_books.push_back(std::move(b));
        }
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(5.0f, 0.9f, 8.0f), 3.0f);
            b->addPage("assets/textures/book_page1.png");
            b->addPage("assets/textures/book_page2.png");
            b->addPage("assets/textures/book_page3.png");
            m_books.push_back(std::move(b));
        }

        m_player.setWorldBounds(-6.0f, 6.0f, -13.0f, 15.0f);

        // Exponential fog: uFogStart=density, uFogEnd=max distance
        renderer.getPostProcess().setFogParams(
            glm::vec3(0.02f, 0.02f, 0.04f), 4.0f, 40.0f, 0.1f, 100.0f);
    }
    else if (m_currentRoom.name == "Artist Studio") {
        // === Third room: intimate artist workspace ===

        // Floor
        auto floor = std::make_unique<PropInstance>();
        floor->mesh = Mesh::createPlane(8.0f, 8.0f);
        floor->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        floor->materialColor = glm::vec3(0.12f, 0.08f, 0.06f); // dark wood floor
        floor->roughness = 0.75f; // worn wood
        m_props.push_back(std::move(floor));

        // Walls (4 sides)
        auto addWall = [&](float x, float z, float ry, float w) {
            auto wall = std::make_unique<PropInstance>();
            wall->mesh = Mesh::createCube(1.0f);
            glm::mat4 t(1.0f);
            t = glm::translate(t, glm::vec3(x, 1.5f, z));
            t = glm::rotate(t, glm::radians(ry), glm::vec3(0, 1, 0));
            t = glm::scale(t, glm::vec3(w, 3.0f, 0.1f));
            wall->transform = t;
            wall->materialColor = glm::vec3(0.08f, 0.06f, 0.05f);
            wall->roughness = 0.85f; // rough plaster walls
            m_props.push_back(std::move(wall));
        };
        addWall(0.0f, -4.0f, 0.0f, 8.0f);   // back wall
        addWall(0.0f, 4.0f, 0.0f, 8.0f);     // front wall
        addWall(-4.0f, 0.0f, 90.0f, 8.0f);   // left wall
        addWall(4.0f, 0.0f, 90.0f, 8.0f);    // right wall

        // Desk
        auto desk = std::make_unique<PropInstance>();
        desk->mesh = Mesh::createCube(1.0f);
        desk->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.4f, -2.0f)), glm::vec3(1.8f, 0.05f, 0.8f));
        desk->materialColor = glm::vec3(0.25f, 0.15f, 0.08f);
        desk->roughness = 0.7f; // wood
        m_props.push_back(std::move(desk));

        // Desk legs
        for (float dx : {-0.8f, 0.8f}) {
            for (float dz : {-0.35f, 0.35f}) {
                auto leg = std::make_unique<PropInstance>();
                leg->mesh = Mesh::createCube(1.0f);
                leg->transform = glm::scale(glm::translate(glm::mat4(1.0f),
                    glm::vec3(dx, 0.2f, -2.0f + dz)), glm::vec3(0.05f, 0.4f, 0.05f));
                leg->materialColor = glm::vec3(0.2f, 0.12f, 0.06f);
                leg->roughness = 0.7f; // wood
                m_props.push_back(std::move(leg));
            }
        }

        // Chair
        auto chair = std::make_unique<PropInstance>();
        chair->mesh = Mesh::createCube(1.0f);
        chair->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.25f, -1.0f)), glm::vec3(0.4f, 0.05f, 0.4f));
        chair->materialColor = glm::vec3(0.3f, 0.18f, 0.1f);
        chair->roughness = 0.7f; // wood
        m_props.push_back(std::move(chair));

        // Ink bottles on desk (small cubes)
        for (float dx : {-0.5f, -0.2f, 0.3f}) {
            auto ink = std::make_unique<PropInstance>();
            ink->mesh = Mesh::createCube(1.0f);
            ink->transform = glm::scale(glm::translate(glm::mat4(1.0f),
                glm::vec3(dx, 0.47f, -2.2f)), glm::vec3(0.04f, 0.06f, 0.04f));
            ink->materialColor = glm::vec3(0.05f, 0.05f, 0.1f); // dark ink
            ink->roughness = 0.15f; // glossy glass bottles
            m_props.push_back(std::move(ink));
        }

        // Shelf on back wall
        auto shelf = std::make_unique<PropInstance>();
        shelf->mesh = Mesh::createCube(1.0f);
        shelf->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 1.5f, -3.9f)), glm::vec3(1.5f, 0.04f, 0.3f));
        shelf->materialColor = glm::vec3(0.2f, 0.13f, 0.07f);
        shelf->roughness = 0.7f; // wood
        m_props.push_back(std::move(shelf));

        // Small artwork on back wall
        {
            auto art = std::make_unique<PropInstance>();
            art->mesh = Mesh::createCube(1.0f);
            glm::mat4 t(1.0f);
            t = glm::translate(t, glm::vec3(2.0f, 2.0f, -3.9f));
            t = glm::scale(t, glm::vec3(1.2f, 1.0f, 0.03f));
            art->transform = t;
            GLuint artTex = generateProceduralArt(200, 256, 256);
            art->texture.setID(artTex);
            m_props.push_back(std::move(art));
        }
        {
            auto art = std::make_unique<PropInstance>();
            art->mesh = Mesh::createCube(1.0f);
            glm::mat4 t(1.0f);
            t = glm::translate(t, glm::vec3(-1.5f, 2.2f, -3.9f));
            t = glm::scale(t, glm::vec3(0.8f, 0.6f, 0.03f));
            art->transform = t;
            GLuint artTex = generateProceduralArt(201, 256, 256);
            art->texture.setID(artTex);
            m_props.push_back(std::move(art));
        }

        // Candle on desk (tiny cube + fire + point light)
        auto candle = std::make_unique<PropInstance>();
        candle->mesh = Mesh::createCube(1.0f);
        candle->transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, 0.48f, -2.0f)), glm::vec3(0.03f, 0.1f, 0.03f));
        candle->materialColor = glm::vec3(0.9f, 0.85f, 0.7f); // white-ish wax
        candle->roughness = 0.3f; // waxy surface
        candle->emissive = glm::vec3(1.5f, 1.1f, 0.5f); // warm glow, >1.0 triggers bloom
        m_props.push_back(std::move(candle));

        m_pointLights.push_back({{0.6f, 0.6f, -2.0f}, {1.0f, 0.8f, 0.4f}, 6.0f, 6.0f, true});

        // Ceiling light (dim, overhead)
        m_pointLights.push_back({{0.0f, 2.8f, 0.0f}, {0.4f, 0.35f, 0.3f}, 8.0f, false});

        // Trigger back to gallery
        TriggerZone back;
        back.position = glm::vec3(0.0f, 0.0f, 3.8f);
        back.radius = 1.5f;
        back.targetRoom = "assets/rooms/hallway.json";
        back.spawnPos = glm::vec3(0.0f, 0.0f, -14.0f);
        m_triggers.push_back(back);

        // Main portfolio book on desk
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(0.0f, 0.5f, -2.0f), 2.0f);
            b->addPage("assets/textures/book_page1.png");
            b->addPage("assets/textures/book_page2.png");
            b->addPage("assets/textures/book_page3.png");
            m_books.push_back(std::move(b));
        }

        // Book on shelf
        {
            auto b = std::make_unique<Book>();
            b->init(glm::vec3(-2.0f, 1.55f, -3.8f), 1.5f);
            b->addPage("assets/textures/book_page1.png");
            m_books.push_back(std::move(b));
        }

        m_player.setWorldBounds(-3.5f, 3.5f, -3.5f, 3.5f);

        // Intimate fog — close, warm
        // Exponential fog: uFogStart=density, uFogEnd=max distance
        renderer.getPostProcess().setFogParams(
            glm::vec3(0.03f, 0.025f, 0.02f), 6.0f, 15.0f, 0.1f, 100.0f);
    }

    // Copy point lights from room def (if any were in JSON)
    for (const auto& pl : m_currentRoom.pointLights) {
        m_pointLights.push_back(pl);
    }

    // Spawn particle emitters from room JSON
    for (const auto& pdef : m_currentRoom.particles) {
        ParticleEmitter e;
        e.type = parseParticleType(pdef.type);
        e.origin = pdef.origin;
        e.area = pdef.area;
        e.count = pdef.count;
        e.color = pdef.color;
        e.speed = pdef.speed;
        e.baseSize = pdef.baseSize;
        m_particles.addEmitter(e);
    }

    // Spawn FMV overlays from room JSON
    FMVOverlay::initShared();
    for (const auto& fdef : m_currentRoom.fmvOverlays) {
        FMVOverlay overlay;
        overlay.loadFromDef(fdef);
        m_fmvOverlays.push_back(std::move(overlay));
    }

    // Load depth pre-pass geometry (hidden geometry for occlusion)
    if (!m_currentRoom.depthGeometryPath.empty()) {
        m_depthGeometry = std::make_unique<Model>();
        if (!m_depthGeometry->load(m_currentRoom.depthGeometryPath)) {
            std::cerr << "Scene: failed to load depth geometry: " << m_currentRoom.depthGeometryPath << "\n";
            m_depthGeometry.reset();
        }
    }

    // Set mouse mode based on first-person flag
    SDL_SetRelativeMouseMode(m_currentRoom.firstPerson ? SDL_TRUE : SDL_FALSE);

    // Load room-specific ambient audio
    if (m_audio && !m_currentRoom.ambientAudioPath.empty()) {
        m_audio->stopAmbient();
        m_audio->loadAmbient(m_currentRoom.ambientAudioPath);
        m_audio->playAmbient();
    }

    std::cout << "Scene: room '" << m_currentRoom.name << "' loaded — "
              << m_props.size() << " props, " << m_pointLights.size() << " lights, "
              << m_triggers.size() << " triggers, " << m_books.size() << " books, "
              << m_fmvOverlays.size() << " FMV overlays"
              << (m_currentRoom.firstPerson ? " [FP]" : " [Fixed]") << "\n";
    return true;
}

void Scene::update(float dt, const InputManager& input, Renderer& renderer, AudioManager* audio, UIOverlay* /*ui*/) {
    m_audio = audio;  // cache for use in loadRoom (called from transitions)

    m_transition.update(dt);
    if (m_transition.isActive()) return;

    // If a book is open, only update the book (block player movement)
    if (m_activeBook) {
        m_activeBook->update(dt, input);
        if (!m_activeBook->isOpen()) {
            m_activeBook = nullptr;
        }
        return;
    }

    glm::vec3 prevPos = m_player.getPosition();
    m_player.update(dt, input, m_camera);
    m_playerMoving = glm::distance(prevPos, m_player.getPosition()) > 0.001f;

    // Camera update: FP or fixed
    if (m_currentRoom.firstPerson) {
        float eyeHeight = 3.0f + m_player.getHeadBob();
        m_camera.updateFromPlayer(m_player.getPosition(), eyeHeight,
                                  m_player.getSmoothYaw(), m_player.getSmoothPitch());
    }
    // else: camera stays as set from room def (fixed)

    m_propTime += dt;
    m_particles.update(dt);
    for (auto& overlay : m_fmvOverlays) overlay.update(dt);

    // Audio: footsteps, reverb, listener position
    if (audio) {
        audio->updateFootsteps(dt, m_playerMoving);
        // Softer reverb: first-person rooms (hallways) get subtle echo,
        // fixed-cam rooms (studios) get minimal reverb
        if (m_currentRoom.firstPerson) {
            audio->setReverb(true, 0.15f, 300.0f);
        } else {
            audio->setReverb(false, 0.1f, 200.0f);
        }

        // Update listener for spatial audio
        glm::vec3 lookDir = m_player.getLookDirection();
        glm::vec3 rightDir = glm::normalize(glm::cross(lookDir, glm::vec3(0, 1, 0)));
        audio->setListenerPos(m_camera.getPosition(), lookDir, rightDir);
    }

    // Check book proximity / look-at
    m_nearBookIndex = -1;
    glm::vec3 camPos = m_camera.getPosition();
    glm::vec3 lookDir = m_player.getLookDirection();

    for (int i = 0; i < static_cast<int>(m_books.size()); i++) {
        if (!m_books[i]->isPlayerNear(m_player.getPosition())) continue;

        if (m_currentRoom.firstPerson) {
            // FP mode: must also be looking at the book
            glm::vec3 toBook = glm::normalize(m_books[i]->getPosition() - camPos);
            float dotAngle = glm::dot(lookDir, toBook);
            // cos(30 deg) ~= 0.866
            if (dotAngle > 0.866f) {
                m_nearBookIndex = i;
                break;
            }
        } else {
            // Fixed-cam: proximity only
            m_nearBookIndex = i;
            break;
        }
    }

    // Smooth highlight animation
    float targetHighlight = (m_nearBookIndex >= 0) ? 1.0f : 0.0f;
    m_highlightAmount += (targetHighlight - m_highlightAmount) * std::min(dt * 8.0f, 1.0f);

    // E to interact with nearby book
    if (m_nearBookIndex >= 0 && input.isKeyPressed(SDL_SCANCODE_E)) {
        m_books[m_nearBookIndex]->open();
        m_activeBook = m_books[m_nearBookIndex].get();
    }

    // Animate rotating props
    for (auto& prop : m_props) {
        if (prop->rotationSpeed != 0.0f) {
            glm::vec3 pos = glm::vec3(prop->transform[3]);
            glm::vec3 scl = glm::vec3(
                glm::length(glm::vec3(prop->transform[0])),
                glm::length(glm::vec3(prop->transform[1])),
                glm::length(glm::vec3(prop->transform[2]))
            );
            float bob = std::sin(m_propTime * 2.0f) * 0.1f;
            glm::mat4 t(1.0f);
            t = glm::translate(t, pos + glm::vec3(0.0f, bob, 0.0f));
            t = glm::rotate(t, m_propTime * prop->rotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
            t = glm::scale(t, scl);
            prop->transform = t;
        }
    }

    // Flicker point lights
    for (auto& light : m_pointLights) {
        if (light.flicker) {
            float flicker = 0.9f + 0.1f * std::sin(m_propTime * 8.0f + light.position.x * 7.0f)
                          + 0.05f * std::sin(m_propTime * 13.0f + light.position.z * 11.0f);
            light.radius = light.baseRadius * flicker;
        }
    }

    // After a room load, wait until player leaves all trigger zones before re-arming
    if (m_triggerCooldown > 0.0f) {
        bool insideAny = false;
        for (const auto& trigger : m_triggers) {
            float dist = glm::distance(glm::vec2(m_player.getPosition().x, m_player.getPosition().z),
                                        glm::vec2(trigger.position.x, trigger.position.z));
            if (dist < trigger.radius) {
                insideAny = true;
                break;
            }
        }
        if (!insideAny) {
            m_triggerCooldown = 0.0f;
        }
        return;
    }

    // Check trigger zones
    for (const auto& trigger : m_triggers) {
        float dist = glm::distance(glm::vec2(m_player.getPosition().x, m_player.getPosition().z),
                                    glm::vec2(trigger.position.x, trigger.position.z));
        if (dist < trigger.radius) {
            std::string targetRoom = trigger.targetRoom;
            glm::vec3 spawnPos = trigger.spawnPos;
            m_transition.startTransition(1.5f, [this, targetRoom, spawnPos, &renderer]() {
                loadRoom(targetRoom, renderer);
                m_player.setPosition(spawnPos);
                m_triggerCooldown = 1.0f;
            });
            break;
        }
    }
}

void Scene::renderObjects() {
    m_meshShader.use();
    m_meshShader.setMat4("uView", glm::value_ptr(m_camera.getView()));
    m_meshShader.setMat4("uProjection", glm::value_ptr(m_camera.getProjection()));

    // Camera position for specular
    glm::vec3 camPos = m_camera.getPosition();
    m_meshShader.setVec3("uViewPos", camPos.x, camPos.y, camPos.z);

    // Directional light
    m_meshShader.setVec3("uAmbient", m_currentRoom.ambientColor.x, m_currentRoom.ambientColor.y, m_currentRoom.ambientColor.z);
    m_meshShader.setVec3("uLightDir", m_currentRoom.lightDir.x, m_currentRoom.lightDir.y, m_currentRoom.lightDir.z);
    m_meshShader.setVec3("uLightColor", m_currentRoom.lightColor.x, m_currentRoom.lightColor.y, m_currentRoom.lightColor.z);

    // Point lights
    int numLights = std::min(static_cast<int>(m_pointLights.size()), 8);
    m_meshShader.setInt("uNumPointLights", numLights);
    for (int i = 0; i < numLights; i++) {
        std::string prefix = "uPointLightPos[" + std::to_string(i) + "]";
        m_meshShader.setVec3(prefix, m_pointLights[i].position.x, m_pointLights[i].position.y, m_pointLights[i].position.z);
        prefix = "uPointLightColor[" + std::to_string(i) + "]";
        m_meshShader.setVec3(prefix, m_pointLights[i].color.x, m_pointLights[i].color.y, m_pointLights[i].color.z);
        prefix = "uPointLightRadius[" + std::to_string(i) + "]";
        m_meshShader.setFloat(prefix, m_pointLights[i].radius);
    }

    // Render props
    for (const auto& prop : m_props) {
        m_meshShader.setMat4("uModel", glm::value_ptr(prop->transform));
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(prop->transform)));
        m_meshShader.setMat3("uNormalMatrix", glm::value_ptr(normalMatrix));

        // Highlight for book-linked props
        float highlight = (prop->bookIndex >= 0 && prop->bookIndex == m_nearBookIndex) ? m_highlightAmount : 0.0f;
        m_meshShader.setFloat("uHighlight", highlight);

        m_meshShader.setFloat("uRoughness", prop->roughness);

        if (prop->model) {
            glDisable(GL_CULL_FACE);
            prop->model->draw(m_meshShader);
            glEnable(GL_CULL_FACE);
        } else {
            m_meshShader.setVec3("uMaterialColor", prop->materialColor.x, prop->materialColor.y, prop->materialColor.z);
            m_meshShader.setVec3("uEmissive", prop->emissive.x, prop->emissive.y, prop->emissive.z);
            m_meshShader.setInt("uHasNormalMap", 0);

            bool hasTex = prop->texture.getID() != 0;
            m_meshShader.setInt("uHasTexture", hasTex ? 1 : 0);
            if (hasTex) {
                prop->texture.bind(GL_TEXTURE0);
                m_meshShader.setInt("uDiffuse", 0);
            }

            prop->mesh.draw();
        }
    }

    // Render player cube only in fixed-camera rooms
    if (!m_currentRoom.firstPerson) {
        m_meshShader.setVec3("uMaterialColor", 0.6f, 0.3f, 0.2f);
        m_meshShader.setInt("uHasTexture", 0);
        m_meshShader.setVec3("uEmissive", 0.0f, 0.0f, 0.0f);
        m_meshShader.setInt("uHasNormalMap", 0);
        m_player.render(m_meshShader);
    }
}

void Scene::renderFMVOverlays() {
    if (m_fmvOverlays.empty()) return;

    glDisable(GL_DEPTH_TEST);
    for (auto& overlay : m_fmvOverlays) {
        overlay.render();
    }
    glEnable(GL_DEPTH_TEST);
}

void Scene::renderParticles() {
    m_particles.render(m_camera.getView(), m_camera.getProjection());
}

void Scene::renderOverlays() {
    m_transition.render();
}

void Scene::renderUI(UIOverlay& ui, int screenWidth, int screenHeight) {
    // Crosshair in FP mode (only when no book open)
    if (m_currentRoom.firstPerson && !m_activeBook) {
        ui.drawCrosshair();
    }

    // Show interaction prompt when looking at a book
    if (m_nearBookIndex >= 0 && !m_activeBook) {
        ui.drawPrompt("Press E to read");
    }

    // Render active book viewer
    if (m_activeBook) {
        m_activeBook->renderUI(ui, screenWidth, screenHeight);
    }
}

void Scene::renderShadowCasters(Shader& shadowShader) {
    // Render all props into shadow map
    for (const auto& prop : m_props) {
        shadowShader.setMat4("uModel", glm::value_ptr(prop->transform));
        if (prop->model) {
            prop->model->draw(shadowShader);
        } else {
            prop->mesh.draw();
        }
    }

    // Render player shadow only in fixed-camera mode
    if (!m_currentRoom.firstPerson) {
        m_player.render(shadowShader);
    }
}

void Scene::renderDepthGeometry(Shader& depthShader, Camera& camera) {
    if (!m_depthGeometry) return;

    depthShader.use();
    depthShader.setMat4("uView", glm::value_ptr(camera.getView()));
    depthShader.setMat4("uProjection", glm::value_ptr(camera.getProjection()));

    // Write depth only, no color
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glm::mat4 identity(1.0f);
    depthShader.setMat4("uModel", glm::value_ptr(identity));
    m_depthGeometry->draw(depthShader);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Scene::bindShadowUniforms(ShadowMap& shadowMap) {
    m_meshShader.use();

    // Bind shadow map to texture unit 1
    shadowMap.bind(GL_TEXTURE1);
    m_meshShader.setInt("uShadowMap", 1);
    m_meshShader.setMat4("uLightSpaceMatrix",
                         glm::value_ptr(shadowMap.getLightSpaceMatrix()));
    m_meshShader.setInt("uUseShadows", 1);
}

GLuint Scene::generateProceduralArt(int seed, int width, int height) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<unsigned char> pixels(width * height * 3);

    // Choose a color palette based on seed
    float hue = dist(gen);
    float baseR = 0.05f + hue * 0.15f;
    float baseG = 0.03f + (1.0f - hue) * 0.1f;
    float baseB = 0.08f + dist(gen) * 0.12f;

    // Generate abstract pattern: layered circles and gradients
    float cx1 = dist(gen), cy1 = dist(gen);
    float cx2 = dist(gen), cy2 = dist(gen);
    float cx3 = dist(gen), cy3 = dist(gen);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float u = static_cast<float>(x) / width;
            float v = static_cast<float>(y) / height;

            float d1 = std::sqrt((u - cx1) * (u - cx1) + (v - cy1) * (v - cy1));
            float d2 = std::sqrt((u - cx2) * (u - cx2) + (v - cy2) * (v - cy2));
            float d3 = std::sqrt((u - cx3) * (u - cx3) + (v - cy3) * (v - cy3));

            float pattern = std::sin(d1 * 12.0f) * 0.3f
                          + std::cos(d2 * 8.0f + d1 * 4.0f) * 0.25f
                          + std::sin(d3 * 15.0f) * 0.2f;

            // Add noise
            pattern += (dist(gen) - 0.5f) * 0.1f;

            float r = std::clamp(baseR + pattern * 0.4f + d1 * 0.3f, 0.0f, 1.0f);
            float g = std::clamp(baseG + pattern * 0.3f + d2 * 0.2f, 0.0f, 1.0f);
            float b = std::clamp(baseB + pattern * 0.35f + d3 * 0.25f, 0.0f, 1.0f);

            // Dark border (frame edge)
            float border = std::min(std::min(u, v), std::min(1.0f - u, 1.0f - v));
            float borderFade = std::clamp(border * 15.0f, 0.0f, 1.0f);
            r *= borderFade;
            g *= borderFade;
            b *= borderFade;

            int idx = (y * width + x) * 3;
            pixels[idx + 0] = static_cast<unsigned char>(r * 255);
            pixels[idx + 1] = static_cast<unsigned char>(g * 255);
            pixels[idx + 2] = static_cast<unsigned char>(b * 255);
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return tex;
}

void Scene::shutdown() {
    m_particles.shutdown();
    m_fmvOverlays.clear();
    FMVOverlay::shutdownShared();
    m_transition.shutdown();
    m_props.clear();
    m_triggers.clear();
    m_pointLights.clear();
}

} // namespace dw
