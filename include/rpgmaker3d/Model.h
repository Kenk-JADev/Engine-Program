#pragma once

#include <string>
#include <vector>
#include "Types.h"

typedef unsigned int GLuint;

namespace rpg {

class Mesh {
public:
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
    void Draw() const;
    void Delete();

private:
    std::vector<Mesh> mMeshes;
};

class MeshFactory {
public:
    static Mesh CreateCube(float size = 1.0f);
    static Mesh CreatePlane(float size = 1.0f);
    static Mesh CreateGrid(int lines, float spacing);
};

} // namespace rpg
