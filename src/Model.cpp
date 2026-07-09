#include "rpgmaker3d/Model.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

namespace rpg {

void Mesh::BuildGPU() {
    if (mVAO) Delete();

    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

void Mesh::Draw() const {
    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::Delete() {
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
    if (mEBO) glDeleteBuffers(1, &mEBO);
    mVAO = mVBO = mEBO = 0;
}

Model::Model() = default;

Model::~Model() {
    Delete();
}

bool Model::LoadFromOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return false;
    }

    Mesh mesh;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vec3 v;
            iss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        } else if (prefix == "vn") {
            Vec3 n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (prefix == "vt") {
            Vec2 t;
            iss >> t.x >> t.y;
            uvs.push_back(t);
        } else if (prefix == "f") {
            std::string face;
            std::vector<unsigned int> faceIndices;
            while (iss >> face) {
                std::replace(face.begin(), face.end(), '/', ' ');
                std::istringstream fss(face);
                int p = 0, t = 0, n = 0;
                fss >> p >> t >> n;

                Vertex v;
                v.position = positions[p - 1];
                v.normal = n > 0 ? normals[n - 1] : Vec3(0, 1, 0);
                v.uv = t > 0 ? uvs[t - 1] : Vec2(0, 0);

                mesh.vertices.push_back(v);
                faceIndices.push_back(static_cast<unsigned int>(mesh.vertices.size() - 1));
            }
            // Triangulate simple convex faces (fan triangulation)
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                mesh.indices.push_back(faceIndices[0]);
                mesh.indices.push_back(faceIndices[i]);
                mesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    if (mesh.vertices.empty()) {
        std::cerr << "OBJ file has no geometry: " << path << std::endl;
        return false;
    }

    mesh.BuildGPU();
    mMeshes.push_back(std::move(mesh));
    return true;
}

void Model::AddMesh(Mesh&& mesh) {
    mMeshes.push_back(std::move(mesh));
}

void Model::Draw() const {
    for (const auto& mesh : mMeshes) {
        mesh.Draw();
    }
}

void Model::Delete() {
    for (auto& mesh : mMeshes) {
        mesh.Delete();
    }
    mMeshes.clear();
}

Mesh MeshFactory::CreateCube(float size) {
    Mesh mesh;
    float h = size * 0.5f;

    mesh.vertices = {
        // Front
        {{-h, -h,  h}, {0, 0, 1}, {0, 0}}, {{ h, -h,  h}, {0, 0, 1}, {1, 0}}, {{ h,  h,  h}, {0, 0, 1}, {1, 1}}, {{-h,  h,  h}, {0, 0, 1}, {0, 1}},
        // Back
        {{ h, -h, -h}, {0, 0, -1}, {0, 0}}, {{-h, -h, -h}, {0, 0, -1}, {1, 0}}, {{-h,  h, -h}, {0, 0, -1}, {1, 1}}, {{ h,  h, -h}, {0, 0, -1}, {0, 1}},
        // Top
        {{-h,  h,  h}, {0, 1, 0}, {0, 0}}, {{ h,  h,  h}, {0, 1, 0}, {1, 0}}, {{ h,  h, -h}, {0, 1, 0}, {1, 1}}, {{-h,  h, -h}, {0, 1, 0}, {0, 1}},
        // Bottom
        {{-h, -h, -h}, {0, -1, 0}, {0, 0}}, {{ h, -h, -h}, {0, -1, 0}, {1, 0}}, {{ h, -h,  h}, {0, -1, 0}, {1, 1}}, {{-h, -h,  h}, {0, -1, 0}, {0, 1}},
        // Right
        {{ h, -h,  h}, {1, 0, 0}, {0, 0}}, {{ h, -h, -h}, {1, 0, 0}, {1, 0}}, {{ h,  h, -h}, {1, 0, 0}, {1, 1}}, {{ h,  h,  h}, {1, 0, 0}, {0, 1}},
        // Left
        {{-h, -h, -h}, {-1, 0, 0}, {0, 0}}, {{-h, -h,  h}, {-1, 0, 0}, {1, 0}}, {{-h,  h,  h}, {-1, 0, 0}, {1, 1}}, {{-h,  h, -h}, {-1, 0, 0}, {0, 1}},
    };

    mesh.indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    mesh.BuildGPU();
    return mesh;
}

Mesh MeshFactory::CreatePlane(float size) {
    Mesh mesh;
    float h = size * 0.5f;
    mesh.vertices = {
        {{-h, 0, -h}, {0, 1, 0}, {0, 0}},
        {{ h, 0, -h}, {0, 1, 0}, {1, 0}},
        {{ h, 0,  h}, {0, 1, 0}, {1, 1}},
        {{-h, 0,  h}, {0, 1, 0}, {0, 1}}
    };
    mesh.indices = {0, 1, 2, 2, 3, 0};
    mesh.BuildGPU();
    return mesh;
}

Mesh MeshFactory::CreateQuad(float width, float height) {
    Mesh mesh;
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    mesh.vertices = {
        {{-hw, -hh, 0}, {0, 0, 1}, {0, 0}},
        {{ hw, -hh, 0}, {0, 0, 1}, {1, 0}},
        {{ hw,  hh, 0}, {0, 0, 1}, {1, 1}},
        {{-hw,  hh, 0}, {0, 0, 1}, {0, 1}}
    };
    mesh.indices = {0, 1, 2, 2, 3, 0};
    mesh.BuildGPU();
    return mesh;
}

Mesh MeshFactory::CreateGrid(int lines, float spacing) {
    Mesh mesh;
    float half = lines * spacing * 0.5f;
    for (int i = 0; i <= lines; ++i) {
        float p = -half + i * spacing;
        Vertex v1{{-half, 0, p}, {0, 1, 0}, {0, 0}};
        Vertex v2{{half, 0, p}, {0, 1, 0}, {1, 0}};
        Vertex v3{{p, 0, -half}, {0, 1, 0}, {0, 0}};
        Vertex v4{{p, 0, half}, {0, 1, 0}, {1, 0}};

        mesh.vertices.push_back(v1); mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3); mesh.vertices.push_back(v4);

        unsigned int base = static_cast<unsigned int>(mesh.vertices.size()) - 4;
        mesh.indices.push_back(base); mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    }
    mesh.BuildGPU();
    return mesh;
}

} // namespace rpg
