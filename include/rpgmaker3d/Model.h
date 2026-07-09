#pragma once

#include <string>
#include <vector>
#include "Types.h"

typedef unsigned int GLuint;

namespace rpg {

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { Delete(); }

    // Kein Kopieren, nur Verschieben (OpenGL-Handles sind einzigartig)
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept
        : vertices(std::move(other.vertices)),
          indices(std::move(other.indices)),
          textureName(std::move(other.textureName)),
          mVAO(other.mVAO), mVBO(other.mVBO), mEBO(other.mEBO) {
        other.mVAO = other.mVBO = other.mEBO = 0;
    }

    Mesh& operator=(Mesh&& other) noexcept {
        if (this != &other) {
            Delete();
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
            textureName = std::move(other.textureName);
            mVAO = other.mVAO; mVBO = other.mVBO; mEBO = other.mEBO;
            other.mVAO = other.mVBO = other.mEBO = 0;
        }
        return *this;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::string textureName;

    void BuildGPU();
    void Draw() const;
    void Delete();

private:
    GLuint mVAO = 0;
    GLuint mVBO = 0;
    GLuint mEBO = 0;
};

class Model {
public:
    Model();
    ~Model();

    bool LoadFromOBJ(const std::string& path);
    void AddMesh(Mesh&& mesh);
    void Draw() const;
    void Delete();

    size_t GetMeshCount() const { return mMeshes.size(); }
    Mesh& GetMesh(size_t index) { return mMeshes[index]; }
    const Mesh& GetMesh(size_t index) const { return mMeshes[index]; }

private:
    std::vector<Mesh> mMeshes;
};

class MeshFactory {
public:
    static Mesh CreateCube(float size = 1.0f);
    static Mesh CreatePlane(float size = 1.0f);
    static Mesh CreateQuad(float width = 1.0f, float height = 1.0f);
    static Mesh CreateGrid(int lines, float spacing);
};

} // namespace rpg
