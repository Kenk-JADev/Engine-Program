#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Shader.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Model.h"
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

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void Renderer::Shutdown() {
    mDefaultShader.reset();
    mDefaultTexture.reset();
}

void Renderer::BeginFrame(const Camera& camera) {
    glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uView", camera.GetViewMatrix());
    mDefaultShader->SetMat4("uProjection", camera.GetProjectionMatrix());
    mDefaultShader->SetVec3("uLightDir", Vec3(0.3f, -1.0f, 0.5f));
    mDefaultShader->SetFloat("uAmbient", 0.35f);
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

} // namespace rpg
