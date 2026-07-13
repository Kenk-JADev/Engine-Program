#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Shader.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"
#include <glad/gl.h>
#include <iostream>
#include <array>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
out float FogFactor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec4 uColor;
uniform float uFogStart;
uniform float uFogEnd;

void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aTexCoord;
    TintColor = uColor;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
    
    // Fog factor calculation in vertex shader (per-vertex fog, cheaper)
    float dist = length((uView * vec4(FragPos, 1.0)).xyz);
    FogFactor = clamp((uFogEnd - dist) / (uFogEnd - uFogStart), 0.0, 1.0);
}
)";

const char* defaultFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 TintColor;
in float FogFactor;

uniform sampler2D uTexture;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform vec3 uFogColor;
uniform bool uFogEnabled;

void main() {
    vec4 texColor = texture(uTexture, TexCoord) * TintColor;
    if (texColor.a < 0.05) discard;

    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -normalize(uLightDir)), 0.0);
    float light = uAmbient + diff * (1.0 - uAmbient);

    vec3 finalColor = texColor.rgb * light;
    
    // Fog blending
    if (uFogEnabled) {
        finalColor = mix(uFogColor, finalColor, FogFactor);
    }

    FragColor = vec4(finalColor, texColor.a);
}
)";

// Skybox shader
const char* skyboxVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    TexCoords = aPos;
    vec4 pos = uProjection * uView * vec4(aPos, 1.0);
    gl_Position = pos.xyww;  // Skybox trick: set w = z for infinite distance
}
)";

const char* skyboxFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube uSkybox;

void main() {
    FragColor = texture(uSkybox, TexCoords);
}
)";

Renderer::Renderer() : mWireframeEnabled(false) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    mDefaultShader = std::make_unique<Shader>();
    if (!mDefaultShader->LoadFromSource(defaultVertexShader, defaultFragmentShader)) {
        std::cerr << "Failed to load default shader" << std::endl;
        return false;
    }

    // Skybox shader
    mSkyboxShader = std::make_unique<Shader>();
    if (!mSkyboxShader->LoadFromSource(skyboxVertexShader, skyboxFragmentShader)) {
        std::cerr << "Failed to load skybox shader" << std::endl;
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
    mSkyboxShader.reset();
    mDefaultTexture.reset();
    mParticleMesh.reset();
    mBoundingBoxMesh.reset();
    if (mSkybox.GetTextureID()) {
        glDeleteTextures(1, &mSkybox.GetTextureID());
        mSkybox.SetTextureID(0);
    }
    if (mSkybox.GetVAO()) {
        glDeleteVertexArrays(1, &mSkybox.GetVAO());
        mSkybox.SetVAO(0);
    }
    if (mSkybox.GetVBO()) {
        glDeleteBuffers(1, &mSkybox.GetVBO());
        mSkybox.SetVBO(0);
    }
}

void Renderer::BeginFrame(const Camera& camera) {
    // Clear with fog color if fog enabled, otherwise clear color
    Color clearCol = mFog.enabled ? mFog.color : mClearColor;
    glClearColor(clearCol.r, clearCol.g, clearCol.b, clearCol.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uView", camera.GetViewMatrix());
    mDefaultShader->SetMat4("uProjection", camera.GetProjectionMatrix());
    UpdateLighting();
    UpdateFogUniforms();
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
    mWireframeEnabled = enable;
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

    // Blending state tracking – default renderer state is BLEND ON
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (material.transparent) {
        if (!blendWasEnabled) glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        // Opaque material: disable blending temporarily for correct depth
        if (blendWasEnabled) glDisable(GL_BLEND);
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
    // Restore blend state to renderer default (ON)
    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    // Ensure default blend func is restored
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

    // Static cache – rebuild only if bounds changed
    static Vec3 lastMin(9999.0f), lastMax(-9999.0f);
    bool boundsChanged = (min != lastMin) || (max != lastMax);
    
    if (boundsChanged || mBoundingBoxMesh->vertices.empty()) {
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
        lastMin = min;
        lastMax = max;
    }

    // Draw as lines
    GLint oldPolygonMode[2];
    glGetIntegerv(GL_POLYGON_MODE, oldPolygonMode);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_DEPTH_TEST); // always on top in editor
    DrawMesh(*mBoundingBoxMesh, transform, nullptr, color);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, oldPolygonMode[0]);
}

void Renderer::UpdateFogUniforms() const {
    if (mDefaultShader) {
        mDefaultShader->SetBool("uFogEnabled", mFog.enabled);
        mDefaultShader->SetVec3("uFogColor", Vec3(mFog.color.r, mFog.color.g, mFog.color.b));
        mDefaultShader->SetFloat("uFogStart", mFog.start);
        mDefaultShader->SetFloat("uFogEnd", mFog.end);
    }
}

// ==================== Skybox Implementation ====================

Skybox::~Skybox() {
    if (mTextureID) glDeleteTextures(1, &mTextureID);
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
}

bool Skybox::Load(const std::array<std::string, 6>& faces) {
    // Create cubemap texture
    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureID);
    
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);  // Cubemaps shouldn't be flipped
    
    for (unsigned int i = 0; i < 6; i++) {
        unsigned char* data = stbi_load(faces.at(i).c_str(), &width, &height, &channels, 0);
        if (data) {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "Failed to load skybox face: " << faces.at(i) << std::endl;
            stbi_image_free(data);
            return false;
        }
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    // Create VAO for skybox cube
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    return true;
}

bool Skybox::LoadFromEquirectangular(const std::string& path) {
    // TODO: Implement HDR panorama to cubemap conversion
    // For now, just return false
    (void)path;
    return false;
}

void Skybox::Draw(const Camera& camera, Shader& shader) {
    if (!mTextureID) return;
    
    // Disable depth writing for skybox
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    
    shader.Bind();
    
    // Create view matrix without translation (skybox stays centered)
    Mat4 view = Mat4(glm::mat3(camera.GetViewMatrix()));  // Remove translation
    
    // Apply rotation if needed
    if (glm::length(mRotation) > 0.001f) {
        Mat4 rot = glm::rotate(Mat4(1.0f), mRotation.y, Vec3(0,1,0));
        rot = glm::rotate(rot, mRotation.x, Vec3(1,0,0));
        rot = glm::rotate(rot, mRotation.z, Vec3(0,0,1));
        view = view * rot;
    }
    
    shader.SetMat4("uView", view);
    shader.SetMat4("uProjection", camera.GetProjectionMatrix());
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureID);
    shader.SetInt("uSkybox", 0);
    
    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    // Restore depth settings
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void Renderer::DrawSkybox(const Camera& camera) {
    if (mSkybox.IsLoaded() && mSkyboxShader) {
        mSkybox.Draw(camera, *mSkyboxShader);
    }
}

} // namespace rpg
