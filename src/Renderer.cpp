#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Shader.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"
#include <glad/gl.h>
#include <iostream>

namespace rpg {

const char* defaultVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 TintColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec4 uColor;

void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aTexCoord;
    TintColor = uColor;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
)";

const char* defaultFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 TintColor;

uniform sampler2D uTexture;
uniform vec3 uLightDir;
uniform float uAmbient;

void main() {
    vec4 texColor = texture(uTexture, TexCoord) * TintColor;
    if (texColor.a < 0.05) discard;

    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -normalize(uLightDir)), 0.0);
    float light = uAmbient + diff * (1.0 - uAmbient);

    FragColor = vec4(texColor.rgb * light, texColor.a);
}
)";

Renderer::Renderer() = default;

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    mDefaultShader = std::make_unique<Shader>();
    if (!mDefaultShader->LoadFromSource(defaultVertexShader, defaultFragmentShader)) {
        std::cerr << "Failed to load default shader" << std::endl;
        return false;
    }

    mDefaultTexture = std::make_unique<Texture>();
    mDefaultTexture->CreateCheckerboard();

    mParticleMesh = std::make_unique<Mesh>(MeshFactory::CreateCube(0.1f));
    mBoundingBoxMesh = std::make_unique<Mesh>();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void Renderer::Shutdown() {
    mDefaultShader.reset();
    mDefaultTexture.reset();
    mParticleMesh.reset();
    mBoundingBoxMesh.reset();
}

void Renderer::BeginFrame(const Camera& camera) {
    glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uView", camera.GetViewMatrix());
    mDefaultShader->SetMat4("uProjection", camera.GetProjectionMatrix());
    UpdateLighting();
    mDefaultTexture->Bind(0);
}

void Renderer::EndFrame() {
    Flush();
    mDefaultShader->Unbind();
}

void Renderer::SetClearColor(const Color& color) {
    mClearColor = color;
}

void Renderer::Clear() {
    glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::DrawMesh(const Mesh& mesh, const Mat4& transform, Texture* texture, const Color& color) {
    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", color);
    if (texture) {
        texture->Bind(0);
        mDefaultShader->SetInt("uTexture", 0);
    } else {
        mDefaultTexture->Bind(0);
        mDefaultShader->SetInt("uTexture", 0);
    }
    mesh.Draw();
}

void Renderer::DrawModel(const Model& model, const Mat4& transform, Texture* texture) {
    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", Color(1.0f));
    if (texture) {
        texture->Bind(0);
        mDefaultShader->SetInt("uTexture", 0);
    } else {
        mDefaultTexture->Bind(0);
        mDefaultShader->SetInt("uTexture", 0);
    }
    model.Draw();
}

void Renderer::Submit(const RenderCommand& cmd) {
    mCommandQueue.push_back(cmd);
}

void Renderer::Flush() {
    for (const auto& cmd : mCommandQueue) {
        DrawMesh(*cmd.mesh, cmd.transform, cmd.texture, cmd.color);
    }
    mCommandQueue.clear();
}

void Renderer::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void Renderer::EnableDepthTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

void Renderer::EnableWireframe(bool enable) {
    glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL);
}

void Renderer::UpdateLighting() {
    Lighting& light = Lighting::Get();
    mDefaultShader->SetVec3("uLightDir", light.GetEffectiveLightDir());
    mDefaultShader->SetFloat("uAmbient", light.GetEffectiveAmbient());
}

void Renderer::SetLightDir(const Vec3& dir) {
    Lighting::Get().GetDirectionalLight().direction = dir;
}

void Renderer::SetAmbient(float ambient) {
    Lighting::Get().GetAmbient().intensity = ambient;
}

void Renderer::DrawMeshWithMaterial(const Mesh& mesh, const Mat4& transform, const Material& material) {
    bool wasWireframe = false;
    if (material.wireframe) {
        GLint polygonMode[2];
        glGetIntegerv(GL_POLYGON_MODE, polygonMode);
        wasWireframe = (polygonMode[0] == GL_LINE);
        EnableWireframe(true);
    }

    if (material.transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", material.diffuse);
    if (material.texture) {
        material.texture->Bind(0);
        mDefaultShader->SetInt("uTexture", 0);
    } else {
        mDefaultTexture->Bind(0);
        mDefaultShader->SetInt("uTexture", 0);
    }
    mesh.Draw();

    if (material.wireframe && !wasWireframe) {
        EnableWireframe(false);
    }
    if (material.transparent) {
        glDisable(GL_BLEND);
    }
}

void Renderer::DrawParticles(const std::vector<Particle>& particles) {
    if (!mParticleMesh) return;
    for (const auto& p : particles) {
        Mat4 transform = glm::translate(Mat4(1.0f), p.position) * glm::scale(Mat4(1.0f), Vec3(p.size));
        DrawMesh(*mParticleMesh, transform, nullptr, p.color);
    }
}

void Renderer::DrawBoundingBox(const Vec3& min, const Vec3& max, const Mat4& transform, const Color& color) {
    if (!mBoundingBoxMesh) return;
    mBoundingBoxMesh->vertices = {
        {{min.x, min.y, min.z}, {0,0,0}, {0,0}}, {{max.x, min.y, min.z}, {0,0,0}, {0,0}},
        {{max.x, min.y, min.z}, {0,0,0}, {0,0}}, {{max.x, min.y, max.z}, {0,0,0}, {0,0}},
        {{max.x, min.y, max.z}, {0,0,0}, {0,0}}, {{min.x, min.y, max.z}, {0,0,0}, {0,0}},
        {{min.x, min.y, max.z}, {0,0,0}, {0,0}}, {{min.x, min.y, min.z}, {0,0,0}, {0,0}},
        {{min.x, max.y, min.z}, {0,0,0}, {0,0}}, {{max.x, max.y, min.z}, {0,0,0}, {0,0}},
        {{max.x, max.y, min.z}, {0,0,0}, {0,0}}, {{max.x, max.y, max.z}, {0,0,0}, {0,0}},
        {{max.x, max.y, max.z}, {0,0,0}, {0,0}}, {{min.x, max.y, max.z}, {0,0,0}, {0,0}},
        {{min.x, max.y, max.z}, {0,0,0}, {0,0}}, {{min.x, max.y, min.z}, {0,0,0}, {0,0}},
        {{min.x, min.y, min.z}, {0,0,0}, {0,0}}, {{min.x, max.y, min.z}, {0,0,0}, {0,0}},
        {{max.x, min.y, min.z}, {0,0,0}, {0,0}}, {{max.x, max.y, min.z}, {0,0,0}, {0,0}},
        {{max.x, min.y, max.z}, {0,0,0}, {0,0}}, {{max.x, max.y, max.z}, {0,0,0}, {0,0}},
        {{min.x, min.y, max.z}, {0,0,0}, {0,0}}, {{min.x, max.y, max.z}, {0,0,0}, {0,0}},
    };
    mBoundingBoxMesh->indices.clear();
    for (size_t i = 0; i < mBoundingBoxMesh->vertices.size(); ++i) {
        mBoundingBoxMesh->indices.push_back(static_cast<unsigned int>(i));
    }
    mBoundingBoxMesh->BuildGPU();
    EnableWireframe(true);
    DrawMesh(*mBoundingBoxMesh, transform, nullptr, color);
    EnableWireframe(false);
}

} // namespace rpg
