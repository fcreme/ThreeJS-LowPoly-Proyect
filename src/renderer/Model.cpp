#include "renderer/Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <filesystem>

namespace dw {

static std::string resolveTexturePath(const std::string& modelDir, const aiString& aiPath) {
    std::string texPath(aiPath.C_Str());
    // If absolute or already exists, use as-is
    if (std::filesystem::exists(texPath))
        return texPath;
    // Try relative to model directory
    std::string resolved = modelDir + "/" + texPath;
    if (std::filesystem::exists(resolved))
        return resolved;
    // Try just the filename (strip directory from texture path)
    auto fname = std::filesystem::path(texPath).filename().string();
    resolved = modelDir + "/" + fname;
    if (std::filesystem::exists(resolved))
        return resolved;
    return texPath; // return original, let Texture::load() report the error
}

bool Model::load(const std::string& path) {
    Assimp::Importer importer;

    unsigned int flags = aiProcess_Triangulate
                       | aiProcess_GenSmoothNormals
                       | aiProcess_FlipUVs
                       | aiProcess_CalcTangentSpace
                       | aiProcess_JoinIdenticalVertices;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "Model: Assimp error loading '" << path << "': " << importer.GetErrorString() << "\n";
        return false;
    }

    std::string modelDir = std::filesystem::path(path).parent_path().string();

    // Pre-load embedded textures (for GLB/FBX with packed textures)
    std::vector<Texture> embeddedTextures(scene->mNumTextures);
    for (unsigned int i = 0; i < scene->mNumTextures; i++) {
        const aiTexture* aiTex = scene->mTextures[i];
        if (aiTex->mHeight == 0) {
            // Compressed (e.g. PNG/JPG embedded) — mWidth = size in bytes
            if (!embeddedTextures[i].loadFromMemory(
                    reinterpret_cast<const unsigned char*>(aiTex->pcData),
                    static_cast<int>(aiTex->mWidth))) {
                std::cerr << "Model: failed to load embedded texture " << i << "\n";
            }
        }
        // Non-compressed embedded textures (raw ARGB) are rare; skip for now
    }

    // Process all meshes
    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++) {
        const aiMesh* aiM = scene->mMeshes[mi];

        std::vector<Vertex> vertices(aiM->mNumVertices);
        for (unsigned int v = 0; v < aiM->mNumVertices; v++) {
            vertices[v].position = glm::vec3(aiM->mVertices[v].x, aiM->mVertices[v].y, aiM->mVertices[v].z);

            if (aiM->mNormals)
                vertices[v].normal = glm::vec3(aiM->mNormals[v].x, aiM->mNormals[v].y, aiM->mNormals[v].z);
            else
                vertices[v].normal = glm::vec3(0.0f, 1.0f, 0.0f);

            if (aiM->mTextureCoords[0])
                vertices[v].texcoord = glm::vec2(aiM->mTextureCoords[0][v].x, aiM->mTextureCoords[0][v].y);

            if (aiM->mTangents)
                vertices[v].tangent = glm::vec3(aiM->mTangents[v].x, aiM->mTangents[v].y, aiM->mTangents[v].z);
            else
                vertices[v].tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        // Indices
        std::vector<uint32_t> indices;
        for (unsigned int f = 0; f < aiM->mNumFaces; f++) {
            const aiFace& face = aiM->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        auto submesh = std::make_unique<SubMesh>();
        submesh->mesh.upload(vertices, indices);

        // Extract material
        if (aiM->mMaterialIndex < scene->mNumMaterials) {
            const aiMaterial* mat = scene->mMaterials[aiM->mMaterialIndex];

            // Base color
            aiColor4D diffuse;
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                submesh->baseColor = glm::vec3(diffuse.r, diffuse.g, diffuse.b);
            }
            // PBR base color factor (glTF)
            aiColor4D baseColor;
            if (mat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
                submesh->baseColor = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
            }

            // PBR roughness (glTF)
            float roughness;
            if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
                submesh->roughness = roughness;
            }

            // Diffuse texture
            if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texPath;
                mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
                std::string tp(texPath.C_Str());

                // Check if it's an embedded texture reference (e.g. "*0", "*1")
                if (!tp.empty() && tp[0] == '*') {
                    unsigned int texIdx = std::stoi(tp.substr(1));
                    if (texIdx < scene->mNumTextures && embeddedTextures[texIdx].getID() != 0) {
                        submesh->texture = std::move(embeddedTextures[texIdx]);
                    }
                } else {
                    std::string resolved = resolveTexturePath(modelDir, texPath);
                    submesh->texture.load(resolved);
                }
            }
            // Also try BASE_COLOR texture (glTF PBR)
            else if (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
                aiString texPath;
                mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath);
                std::string tp(texPath.C_Str());

                if (!tp.empty() && tp[0] == '*') {
                    unsigned int texIdx = std::stoi(tp.substr(1));
                    if (texIdx < scene->mNumTextures && embeddedTextures[texIdx].getID() != 0) {
                        submesh->texture = std::move(embeddedTextures[texIdx]);
                    }
                } else {
                    std::string resolved = resolveTexturePath(modelDir, texPath);
                    submesh->texture.load(resolved);
                }
            }

            // Normal map (try NORMALS first, then HEIGHT as fallback)
            auto loadNormalMap = [&](aiTextureType type) -> bool {
                if (mat->GetTextureCount(type) > 0) {
                    aiString texPath;
                    mat->GetTexture(type, 0, &texPath);
                    std::string tp(texPath.C_Str());
                    if (!tp.empty() && tp[0] == '*') {
                        unsigned int texIdx = std::stoi(tp.substr(1));
                        if (texIdx < scene->mNumTextures) {
                            // Load a copy from memory for normal map
                            const aiTexture* aiTex = scene->mTextures[texIdx];
                            if (aiTex->mHeight == 0) {
                                submesh->normalMap.loadFromMemory(
                                    reinterpret_cast<const unsigned char*>(aiTex->pcData),
                                    static_cast<int>(aiTex->mWidth));
                            }
                        }
                    } else {
                        std::string resolved = resolveTexturePath(modelDir, texPath);
                        submesh->normalMap.load(resolved);
                    }
                    return submesh->normalMap.getID() != 0;
                }
                return false;
            };
            if (!loadNormalMap(aiTextureType_NORMALS))
                loadNormalMap(aiTextureType_HEIGHT);

            // Emissive color
            aiColor4D emissiveColor;
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
                submesh->emissive = glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
            }
        }

        m_submeshes.push_back(std::move(submesh));
    }

    std::cout << "Model: loaded '" << path << "' (" << m_submeshes.size() << " submeshes)\n";
    return true;
}

void Model::draw(Shader& shader) const {
    for (const auto& sub : m_submeshes) {
        bool hasTex = sub->texture.getID() != 0;
        shader.setInt("uHasTexture", hasTex ? 1 : 0);
        shader.setVec3("uMaterialColor", sub->baseColor.x, sub->baseColor.y, sub->baseColor.z);
        shader.setFloat("uRoughness", sub->roughness);
        if (hasTex) {
            sub->texture.bind(GL_TEXTURE0);
            shader.setInt("uDiffuse", 0);
        }

        // Normal map
        bool hasNormal = sub->normalMap.getID() != 0;
        shader.setInt("uHasNormalMap", hasNormal ? 1 : 0);
        if (hasNormal) {
            sub->normalMap.bind(GL_TEXTURE2);
            shader.setInt("uNormalMap", 2);
        }

        // Emissive
        shader.setVec3("uEmissive", sub->emissive.x, sub->emissive.y, sub->emissive.z);

        sub->mesh.draw();
    }
}

} // namespace dw
