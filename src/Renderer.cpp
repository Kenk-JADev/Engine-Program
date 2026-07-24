#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Shader.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"
#include "rpgmaker3d/ShadowMap.h"
#include "rpgmaker3d/Scene.h"
#include <glad/gl.h>
#include <iostream>
#include <array>
#include <string>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace rpg {

// ---------- Default PBR-ish forward shader with improved shadows ----------
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
out vec4 FragPosLightSpace;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;
uniform vec4 uColor;
uniform float uFogStart;
uniform float uFogEnd;

void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    // Normal matrix
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aTexCoord;
    TintColor = uColor;
    FragPosLightSpace = uLightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
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
in vec4 FragPosLightSpace;

uniform sampler2D uTexture;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform float uAmbient;
uniform vec3 uAmbientColor;
uniform vec3 uFogColor;
uniform bool uFogEnabled;

uniform int uPointLightCount;
uniform vec3 uPointLightPos[4];
uniform vec3 uPointLightColor[4];
uniform float uPointLightIntensity[4];
uniform float uPointLightRange[4];

// Directional shadows
uniform sampler2D uShadowMap;
uniform bool uShadowsEnabled;
uniform float uShadowStrength;
uniform float uShadowBias;
uniform bool uShadowPCF;

// Point shadows cubemap
uniform bool uPointShadowsEnabled;
uniform int uPointShadowCount;
uniform samplerCube uPointShadowMap0;
uniform samplerCube uPointShadowMap1;
uniform vec3 uPointShadowPos0;
uniform vec3 uPointShadowPos1;
uniform float uPointShadowFar0;
uniform float uPointShadowFar1;
uniform float uPointShadowBias;

// Sampling disk for point PCF (20 points)
vec3 gridSamplingDisk[20] = vec3[](
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1),
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3(0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float CalculateDirShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (!uShadowsEnabled) return 0.0;
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;

    float currentDepth = projCoords.z;

    // Normal bias + slope bias to fix corner light leaking / Peter-Panning
    float NdotL = max(dot(normal, -lightDir), 0.0);
    // Increase bias at grazing angles (corners) – key fix for "Licht muss auf Ecken achten"
    float slopeBias = (1.0 - NdotL) * (1.0 - NdotL); // squared for stronger at edges
    float bias = max(uShadowBias * (1.0 + slopeBias * 3.0), 0.0005);
    bias = max(bias, uShadowBias * 0.1);

    // Edge fade near shadow map border to avoid hard cuts
    float borderFade = 1.0;
    float border = 0.05;
    if (projCoords.x < border) borderFade *= projCoords.x / border;
    if (projCoords.x > 1.0 - border) borderFade *= (1.0 - projCoords.x) / border;
    if (projCoords.y < border) borderFade *= projCoords.y / border;
    if (projCoords.y > 1.0 - border) borderFade *= (1.0 - projCoords.y) / border;
    borderFade = clamp(borderFade, 0.0, 1.0);

    float shadow = 0.0;
    if (uShadowPCF) {
        vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
        // 5x5 PCF for softer shadows and less aliasing at corners
        int kernel = 2;
        float count = 0.0;
        for (int x = -kernel; x <= kernel; ++x) {
            for (int y = -kernel; y <= kernel; ++y) {
                float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
                count += 1.0;
            }
        }
        shadow /= count;
    } else {
        float closest = texture(uShadowMap, projCoords.xy).r;
        shadow = currentDepth - bias > closest ? 1.0 : 0.0;
    }
    // Fade shadow at edges
    shadow *= borderFade;
    // Apply strength
    shadow *= uShadowStrength;
    return shadow;
}

float CalculatePointShadow(int idx, vec3 fragPos, vec3 lightPos, vec3 normal, vec3 lightDir) {
    if (!uPointShadowsEnabled) return 0.0;
    if (idx >= uPointShadowCount) return 0.0;

    // GLSL 330 forbids local sampler variables and sampler assignment
    // (opaque types may only be uniforms). Select via plain floats instead;
    // the correct uniform cubemap is sampled directly below. Intel drivers
    // strictly enforce this - the old "samplerCube shadowCube = ..." code
    // failed to compile on Intel GPUs and aborted renderer initialization.
    float farPlane = (idx == 0) ? uPointShadowFar0 : uPointShadowFar1;

    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    // If beyond far plane, no shadow (light doesn't reach)
    if (currentDepth > farPlane) return 0.0;

    float bias = max(uPointShadowBias * (1.0 - max(dot(normal, -lightDir), 0.0)), uPointShadowBias * 0.05);
    // Add distance-dependent bias for point shadows
    bias += 0.01 * (currentDepth / farPlane);

    float shadow = 0.0;
    // PCF with 20 samples
    int samples = 20;
    float viewDistance = length(FragPos);
    float diskRadius = (1.0 + (viewDistance / farPlane)) / 25.0;
    for (int i = 0; i < samples; ++i) {
        vec3 sampleDir = fragToLight + gridSamplingDisk[i] * diskRadius;
        // Sample the correct uniform cubemap directly (see note above).
        float closestDepth;
        if (idx == 0)
            closestDepth = texture(uPointShadowMap0, sampleDir).r;
        else
            closestDepth = texture(uPointShadowMap1, sampleDir).r;
        closestDepth *= farPlane;
        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
    return shadow;
}

void main() {
    vec4 texColor = texture(uTexture, TexCoord) * TintColor;
    if (texColor.a < 0.05) discard;

    vec3 norm = normalize(Normal);
    vec3 L = normalize(-uLightDir);
    float NdotL = max(dot(norm, L), 0.0);
    float wrap = NdotL * 0.5 + 0.5;
    wrap = wrap * wrap;

    vec3 ambient = uAmbientColor * uAmbient;
    vec3 diffuse = uLightColor * uLightIntensity * mix(NdotL, wrap, 0.25);

    float dirShadow = CalculateDirShadow(FragPosLightSpace, norm, uLightDir);
    diffuse *= (1.0 - dirShadow);

    // Point lights with optional cubemap shadows
    for (int i = 0; i < uPointLightCount; ++i) {
        vec3 toL = uPointLightPos[i] - FragPos;
        float dist = length(toL);
        float range = max(uPointLightRange[i], 0.001);
        if (dist > range) continue;
        float atten = clamp(1.0 - dist / range, 0.0, 1.0);
        atten *= atten;
        vec3 ldir = toL / max(dist, 0.001);
        float nd = max(dot(norm, ldir), 0.0);

        float ptShadow = 0.0;
        if (uPointShadowsEnabled && i < uPointShadowCount) {
            ptShadow = CalculatePointShadow(i, FragPos, uPointLightPos[i], norm, ldir);
        }
        diffuse += uPointLightColor[i] * uPointLightIntensity[i] * nd * atten * (1.0 - ptShadow);
    }

    vec3 finalColor = texColor.rgb * (ambient + diffuse);

    if (uFogEnabled) {
        finalColor = mix(uFogColor, finalColor, FogFactor);
    }

    FragColor = vec4(finalColor, texColor.a);
}
)";

// Directional shadow mapping shaders
const char* shadowVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
void main() {
    // Small normal offset to reduce shadow acne (helps corner leak)
    vec3 normal = aNormal;
    vec4 worldPos = uModel * vec4(aPos + normal * 0.01, 1.0);
    gl_Position = uLightSpaceMatrix * worldPos;
}
)";

const char* shadowFragmentShader = R"(
#version 330 core
void main() {
    // Depth only
}
)";

// Point shadow shaders (cubemap)
const char* pointShadowVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uLightMatrix;
uniform mat4 uModel;
out vec3 FragPos;
void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    gl_Position = uLightMatrix * vec4(FragPos, 1.0);
}
)";

const char* pointShadowFragmentShader = R"(
#version 330 core
in vec3 FragPos;
uniform vec3 uLightPos;
uniform float uFarPlane;
void main() {
    float dist = length(FragPos - uLightPos);
    dist = dist / uFarPlane;
    gl_FragDepth = dist;
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
    gl_Position = pos.xyww;
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

Renderer::Renderer() : mWireframeEnabled(false), mShadowsEnabled(true), mPointShadowsEnabled(true), mShadowMapSize(2048), mPointShadowSize(1024) {}

Renderer::~Renderer() { Shutdown(); }

bool Renderer::Initialize() {
    mDefaultShader = std::make_unique<Shader>();
    if (!mDefaultShader->LoadFromSource(defaultVertexShader, defaultFragmentShader)) {
        std::cerr << "Failed to load default shader" << std::endl;
        return false;
    }
    mSkyboxShader = std::make_unique<Shader>();
    if (!mSkyboxShader->LoadFromSource(skyboxVertexShader, skyboxFragmentShader)) {
        std::cerr << "Failed to load skybox shader" << std::endl;
    }
    mShadowShader = std::make_unique<Shader>();
    if (!mShadowShader->LoadFromSource(shadowVertexShader, shadowFragmentShader)) {
        std::cerr << "Failed to load shadow shader" << std::endl;
    }
    mPointShadowShader = std::make_unique<Shader>();
    if (!mPointShadowShader->LoadFromSource(pointShadowVertexShader, pointShadowFragmentShader)) {
        std::cerr << "Failed to load point shadow shader" << std::endl;
    }

    mDefaultTexture = std::make_unique<Texture>();
    mDefaultTexture->CreateCheckerboard();

    mParticleMesh = std::make_unique<Mesh>(MeshFactory::CreateCube(0.1f));
    mBoundingBoxMesh = std::make_unique<Mesh>();

    if (!InitShadowSystem(mShadowMapSize)) {
        std::cerr << "Failed to init shadow system, shadows disabled" << std::endl;
        mShadowsEnabled = false;
    }
    if (!InitPointShadowSystem(mPointShadowSize)) {
        std::cerr << "Failed to init point shadow system, point shadows disabled" << std::endl;
        mPointShadowsEnabled = false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void Renderer::Shutdown() {
    ShutdownShadowSystem();
    ShutdownPointShadowSystem();
    mDefaultShader.reset();
    mSkyboxShader.reset();
    mShadowShader.reset();
    mPointShadowShader.reset();
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

bool Renderer::InitShadowSystem(int mapSize) {
    mShadowMapSize = mapSize;
    mShadowMap = std::make_unique<ShadowMap>();
    if (!mShadowMap->Create(mapSize, mapSize)) return false;
    mLightSpaceMatrix = CalculateLightSpaceMatrix();
    mShadowMap->SetLightSpaceMatrix(mLightSpaceMatrix);
    return true;
}

void Renderer::ShutdownShadowSystem() {
    if (mShadowMap) {
        mShadowMap->Delete();
        mShadowMap.reset();
    }
}

bool Renderer::InitPointShadowSystem(int cubeSize) {
    mPointShadowSize = cubeSize;
    // Create up to 2 cubemap shadows (limit for performance)
    mPointShadowMaps.clear();
    for (int i = 0; i < 2; ++i) {
        auto cubem = std::make_unique<ShadowCubeMap>();
        if (cubem->Create(cubeSize)) {
            mPointShadowMaps.push_back(std::move(cubem));
        }
    }
    return !mPointShadowMaps.empty();
}

void Renderer::ShutdownPointShadowSystem() {
    for (auto& m : mPointShadowMaps) {
        if (m) m->Delete();
    }
    mPointShadowMaps.clear();
}

Mat4 Renderer::CalculateLightSpaceMatrix(float orthoSize, float nearPlane, float farPlane) {
    Lighting& lighting = Lighting::Get();
    const auto& dirLight = lighting.GetDirectionalLight();
    const auto& shadowSettings = lighting.GetShadows();

    float size = shadowSettings.enabled ? shadowSettings.orthoSize : orthoSize;
    float n = shadowSettings.enabled ? shadowSettings.nearPlane : nearPlane;
    float f = shadowSettings.enabled ? shadowSettings.farPlane : farPlane;

    // Adaptive based on caller (Engine may have already adapted)
    if (size <= 0.0f) size = 30.0f;

    Vec3 lightDir = glm::normalize(dirLight.direction);
    Vec3 center(0.0f, 0.0f, 0.0f);
    Vec3 lightPos = center - lightDir * 25.0f;

    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
    if (abs(glm::dot(lightDir, up)) > 0.99f) up = Vec3(0.0f, 0.0f, 1.0f);

    Mat4 lightView = glm::lookAt(lightPos, center, up);
    Mat4 lightProj = glm::ortho(-size, size, -size, size, n, f);
    Mat4 lightSpace = lightProj * lightView;
    mLightSpaceMatrix = lightSpace;
    mLightSpaceValid = true; // PAKET 27
    if (mShadowMap) mShadowMap->SetLightSpaceMatrix(lightSpace);
    return lightSpace;
}

void Renderer::BeginShadowPass() {
    if (!mShadowsEnabled || !mShadowMap || !mShadowMap->IsValid() || !mShadowShader) return;
    // PAKET 27: KEINE eigene CalculateLightSpaceMatrix()-Berechnung mehr!
    // Die Engine ruft sie direkt zuvor mit karten-adaptiver Ortho-Groesse
    // (kleinere Karte = dichtere Shadow-Matrix = schaerfer). Der zweite
    // Aufruf hier hat diese Matrix sofort wieder mit dem fixen Default
    // (ortho 30) ueberschrieben - die Adaption war wirkungslos.
    // Fallback fuer andere Aufrufer: nur rechnen, wenn noch nie gerechnet
    // wurde (Matrix ist noch Identitaet).
    if (!mLightSpaceValid) {
        CalculateLightSpaceMatrix();
    }
    // WICHTIG (Qt-Fix): aktuelles Host-FBO + Viewport merken. QOpenGLWidget
    // rendert in ein eigenes FBO (!= 0); ein spaeteres BindFramebuffer(..., 0)
    // wuerde die Szene unsichtbar in den Fenster-Backbuffer zeichnen.
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &mPrevDrawFBO);
    glGetIntegerv(GL_VIEWPORT, mPrevViewport);
    mShadowMap->Bind();
    mShadowShader->Bind();
    mShadowShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // Improve shadows: front face culling + polygon offset reduces acne and corner leaking
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.5f, 10.0f);
}

void Renderer::EndShadowPass() {
    if (!mShadowMap) return;
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    mShadowMap->Unbind();
    if (mShadowShader) mShadowShader->Unbind();
    // Host-FBO + Viewport wiederherstellen (Qt-Fix, siehe BeginShadowPass)
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(mPrevDrawFBO));
    glViewport(mPrevViewport[0], mPrevViewport[1], mPrevViewport[2], mPrevViewport[3]);
}

void Renderer::DrawMeshDepth(const Mesh& mesh, const Mat4& transform) {
    if (!mShadowShader || !mShadowMap) return;
    mShadowShader->Bind();
    mShadowShader->SetMat4("uModel", transform);
    mShadowShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    mesh.Draw();
}

void Renderer::DrawModelDepth(const Model& model, const Mat4& transform) {
    if (!mShadowShader) return;
    mShadowShader->Bind();
    mShadowShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    mShadowShader->SetMat4("uModel", transform);
    model.Draw();
}

unsigned int Renderer::GetShadowMapTexture() const {
    if (mShadowMap) return mShadowMap->GetDepthTextureID();
    return 0;
}

unsigned int Renderer::GetPointShadowCubemap(int index) const {
    if (index < 0 || index >= static_cast<int>(mPointShadowMaps.size())) return 0;
    return mPointShadowMaps[index]->GetDepthCubemapID();
}

void Renderer::RenderPointShadows(Scene& scene) {
    if (!mPointShadowsEnabled || !mPointShadowShader || mPointShadowMaps.empty()) return;
    // Host-FBO merken (Qt-Fix): QOpenGLWidget-FBO ist != 0
    int prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    Lighting& lighting = Lighting::Get();
    int shadowIdx = 0;
    for (size_t i = 0; i < lighting.GetPointLightCount() && shadowIdx < static_cast<int>(mPointShadowMaps.size()); ++i) {
        auto& pl = lighting.GetPointLight(i);
        if (!pl.enabled) continue;
        if (!pl.castShadows) continue;

        auto& shadowCube = mPointShadowMaps[shadowIdx];
        if (!shadowCube || !shadowCube->IsValid()) continue;

        float nearP = 0.1f;
        float farP = pl.range > 0.0f ? pl.range : 25.0f;
        auto matrices = shadowCube->GetLightMatrices(pl.position, nearP, farP);

        // Render each face
        for (int face = 0; face < 6; ++face) {
            glBindFramebuffer(GL_FRAMEBUFFER, shadowCube->GetFBO());
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, shadowCube->GetDepthCubemapID(), 0);
            glViewport(0, 0, shadowCube->GetSize(), shadowCube->GetSize());
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 2.0f);

            mPointShadowShader->Bind();
            mPointShadowShader->SetMat4("uLightMatrix", matrices[face]);
            mPointShadowShader->SetVec3("uLightPos", pl.position);
            mPointShadowShader->SetFloat("uFarPlane", farP);

            // Draw all models in scene – including map if visible? For point shadows, we want all
            // We need access to Map – we don't have it here directly, but scene includes its own?
            // Map will be rendered by Engine separately if needed; here we just draw scene entities
            for (EntityID id : scene.GetEntities()) {
                auto* tr = scene.GetComponent<TransformComponent>(id);
                auto* model = scene.GetComponent<ModelRendererComponent>(id);
                if (!tr || !model || !model->model) continue;
                Mat4 mat = tr->transform.GetMatrix();
                mPointShadowShader->SetMat4("uModel", mat);
                for (int mi = 0; mi < model->model->GetMeshCount(); ++mi) {
                    model->model->GetMesh(mi).Draw();
                }
            }

            glDisable(GL_POLYGON_OFFSET_FILL);
            glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);
            mPointShadowShader->Unbind();
        }
        // Zurueck zum Host-FBO (Qt-Fix, NICHT hart 0)
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(prevFBO));
        shadowIdx++;
    }
    // Sicherheit: am Ende nochmals Host-FBO binden
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(prevFBO));
}

void Renderer::BeginFrame(const Camera& camera) {
    if (mShadowsEnabled) CalculateLightSpaceMatrix();
    glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uView", camera.GetViewMatrix());
    mDefaultShader->SetMat4("uProjection", camera.GetProjectionMatrix());
    mDefaultShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    UpdateLighting();
    UpdateFogUniforms();
    UpdateShadowUniforms();
    UpdatePointShadowUniforms();
    mDefaultTexture->Bind(0);
}

void Renderer::EndFrame() {
    Flush();
    mDefaultShader->Unbind();
}

void Renderer::SetClearColor(const Color& color) { mClearColor = color; }
void Renderer::Clear() {
    glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::DrawMesh(const Mesh& mesh, const Mat4& transform, Texture* texture, const Color& color) {
    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", color);
    mDefaultShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    if (texture) texture->Bind(0); else mDefaultTexture->Bind(0);
    mDefaultShader->SetInt("uTexture", 0);
    if (mShadowsEnabled && mShadowMap && mShadowMap->IsValid()) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mShadowMap->GetDepthTextureID());
        mDefaultShader->SetInt("uShadowMap", 1);
        glActiveTexture(GL_TEXTURE0);
    }
    // Bind point shadow cubemaps (units 2,3) if enabled
    if (mPointShadowsEnabled) {
        for (int i = 0; i < GetPointShadowCount() && i < 2; ++i) {
            unsigned int tex = GetPointShadowCubemap(i);
            if (tex) {
                glActiveTexture(GL_TEXTURE2 + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
                if (i == 0) mDefaultShader->SetInt("uPointShadowMap0", 2);
                else mDefaultShader->SetInt("uPointShadowMap1", 3);
            }
        }
        glActiveTexture(GL_TEXTURE0);
    }
    mesh.Draw();
}

void Renderer::DrawModel(const Model& model, const Mat4& transform, Texture* texture) {
    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", Color(1.0f));
    mDefaultShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    if (texture) texture->Bind(0); else mDefaultTexture->Bind(0);
    mDefaultShader->SetInt("uTexture", 0);
    if (mShadowsEnabled && mShadowMap && mShadowMap->IsValid()) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mShadowMap->GetDepthTextureID());
        mDefaultShader->SetInt("uShadowMap", 1);
        glActiveTexture(GL_TEXTURE0);
    }
    if (mPointShadowsEnabled) {
        for (int i = 0; i < GetPointShadowCount() && i < 2; ++i) {
            unsigned int tex = GetPointShadowCubemap(i);
            if (tex) {
                glActiveTexture(GL_TEXTURE2 + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
                if (i == 0) mDefaultShader->SetInt("uPointShadowMap0", 2);
                else mDefaultShader->SetInt("uPointShadowMap1", 3);
            }
        }
        glActiveTexture(GL_TEXTURE0);
    }
    model.Draw();
}

void Renderer::Submit(const RenderCommand& cmd) { mCommandQueue.push_back(cmd); }
void Renderer::Flush() {
    for (const auto& cmd : mCommandQueue) DrawMesh(*cmd.mesh, cmd.transform, cmd.texture, cmd.color);
    mCommandQueue.clear();
}

void Renderer::SetViewport(int x, int y, int w, int h) { glViewport(x, y, w, h); }
void Renderer::EnableDepthTest(bool enable) { if (enable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); }
void Renderer::EnableWireframe(bool enable) { glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL); mWireframeEnabled = enable; }

void Renderer::UpdateLighting() {
    Lighting& light = Lighting::Get();
    const auto& dir = light.GetDirectionalLight();
    const auto& amb = light.GetAmbient();
    Vec3 lightDir = light.GetEffectiveLightDir();
    float intensity = dir.enabled ? dir.intensity : 0.0f;
    mDefaultShader->SetVec3("uLightDir", lightDir);
    mDefaultShader->SetVec3("uLightColor", Vec3(dir.color.r, dir.color.g, dir.color.b));
    mDefaultShader->SetFloat("uLightIntensity", intensity);
    mDefaultShader->SetFloat("uAmbient", amb.intensity);
    mDefaultShader->SetVec3("uAmbientColor", Vec3(amb.color.r, amb.color.g, amb.color.b));

    int count = 0;
    for (size_t i = 0; i < light.GetPointLightCount() && count < 4; ++i) {
        const auto& pl = light.GetPointLight(i);
        if (!pl.enabled) continue;
        std::string idx = std::to_string(count);
        mDefaultShader->SetVec3(("uPointLightPos[" + idx + "]").c_str(), pl.position);
        mDefaultShader->SetVec3(("uPointLightColor[" + idx + "]").c_str(), Vec3(pl.color.r, pl.color.g, pl.color.b));
        mDefaultShader->SetFloat(("uPointLightIntensity[" + idx + "]").c_str(), pl.intensity);
        mDefaultShader->SetFloat(("uPointLightRange[" + idx + "]").c_str(), pl.range);
        ++count;
    }
    mDefaultShader->SetInt("uPointLightCount", count);
}

void Renderer::UpdateShadowUniforms() const {
    if (!mDefaultShader) return;
    Lighting& lighting = Lighting::Get();
    const auto& dir = lighting.GetDirectionalLight();
    const auto& shadow = lighting.GetShadows();
    bool enabled = mShadowsEnabled && shadow.enabled && dir.castShadows && dir.enabled;
    mDefaultShader->SetBool("uShadowsEnabled", enabled);
    mDefaultShader->SetFloat("uShadowStrength", shadow.enabled ? shadow.strength : dir.shadowStrength);
    mDefaultShader->SetFloat("uShadowBias", shadow.enabled ? shadow.bias : dir.shadowBias);
    mDefaultShader->SetBool("uShadowPCF", shadow.pcf);
    mDefaultShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    if (enabled && mShadowMap && mShadowMap->IsValid()) mDefaultShader->SetInt("uShadowMap", 1);
}

void Renderer::UpdatePointShadowUniforms() const {
    if (!mDefaultShader) return;
    Lighting& lighting = Lighting::Get();
    int shadowCount = 0;
    // Count shadow-casting point lights limited to 2
    for (size_t i = 0; i < lighting.GetPointLightCount() && shadowCount < 2; ++i) {
        const auto& pl = lighting.GetPointLight(i);
        if (!pl.enabled) continue;
        if (!pl.castShadows) continue;
        shadowCount++;
    }
    mDefaultShader->SetBool("uPointShadowsEnabled", mPointShadowsEnabled && shadowCount > 0);
    mDefaultShader->SetInt("uPointShadowCount", shadowCount);
    mDefaultShader->SetFloat("uPointShadowBias", 0.02f);

    // Set far planes and positions for up to 2
    int idx = 0;
    for (size_t i = 0; i < lighting.GetPointLightCount() && idx < 2; ++i) {
        const auto& pl = lighting.GetPointLight(i);
        if (!pl.enabled || !pl.castShadows) continue;
        if (idx == 0) {
            mDefaultShader->SetVec3("uPointShadowPos0", pl.position);
            mDefaultShader->SetFloat("uPointShadowFar0", pl.range > 0 ? pl.range : 25.0f);
            if (GetPointShadowCubemap(0)) mDefaultShader->SetInt("uPointShadowMap0", 2);
        } else {
            mDefaultShader->SetVec3("uPointShadowPos1", pl.position);
            mDefaultShader->SetFloat("uPointShadowFar1", pl.range > 0 ? pl.range : 25.0f);
            if (GetPointShadowCubemap(1)) mDefaultShader->SetInt("uPointShadowMap1", 3);
        }
        idx++;
    }
}

void Renderer::SetLightDir(const Vec3& dir) { Lighting::Get().GetDirectionalLight().direction = dir; }
void Renderer::SetAmbient(float ambient) { Lighting::Get().GetAmbient().intensity = ambient; }

void Renderer::DrawMeshWithMaterial(const Mesh& mesh, const Mat4& transform, const Material& material) {
    bool wasWireframe = false;
    if (material.wireframe) {
        GLint pm[2]; glGetIntegerv(GL_POLYGON_MODE, pm);
        wasWireframe = (pm[0] == GL_LINE);
        EnableWireframe(true);
    }
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    if (material.transparent) {
        if (!blendWasEnabled) glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        if (blendWasEnabled) glDisable(GL_BLEND);
    }
    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", material.diffuse);
    mDefaultShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    if (material.texture) material.texture->Bind(0); else mDefaultTexture->Bind(0);
    mDefaultShader->SetInt("uTexture", 0);
    if (mShadowsEnabled && mShadowMap && mShadowMap->IsValid()) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mShadowMap->GetDepthTextureID());
        mDefaultShader->SetInt("uShadowMap", 1);
        glActiveTexture(GL_TEXTURE0);
    }
    if (mPointShadowsEnabled) {
        for (int i = 0; i < GetPointShadowCount() && i < 2; ++i) {
            unsigned int tex = GetPointShadowCubemap(i);
            if (tex) {
                glActiveTexture(GL_TEXTURE2 + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
                if (i == 0) mDefaultShader->SetInt("uPointShadowMap0", 2);
                else mDefaultShader->SetInt("uPointShadowMap1", 3);
            }
        }
        glActiveTexture(GL_TEXTURE0);
    }
    mesh.Draw();
    if (material.wireframe && !wasWireframe) EnableWireframe(false);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::DrawParticles(const std::vector<Particle>& particles) {
    if (!mParticleMesh || particles.empty()) return;
    GLboolean depthMask; glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const auto& p : particles) {
        Mat4 tr = glm::translate(Mat4(1.0f), p.position) * glm::scale(Mat4(1.0f), Vec3(p.size));
        DrawMesh(*mParticleMesh, tr, nullptr, p.color);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(depthMask);
}

void Renderer::DrawGrid(const Mesh& mesh, const Mat4& transform, const Color& color) {
    mDefaultShader->Bind();
    mDefaultShader->SetMat4("uModel", transform);
    mDefaultShader->SetVec4("uColor", color);
    mDefaultShader->SetMat4("uLightSpaceMatrix", mLightSpaceMatrix);
    mDefaultShader->SetBool("uShadowsEnabled", false);
    mDefaultTexture->Bind(0);
    mDefaultShader->SetInt("uTexture", 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    mesh.DrawLines();
    glDepthMask(GL_TRUE);
    UpdateShadowUniforms();
}

void Renderer::DrawBoundingBox(const Vec3& min, const Vec3& max, const Mat4& transform, const Color& color) {
    if (!mBoundingBoxMesh) return;
    static Vec3 lastMin(9999.0f), lastMax(-9999.0f);
    bool changed = (min != lastMin) || (max != lastMax);
    if (changed || mBoundingBoxMesh->vertices.empty()) {
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
        for (size_t i = 0; i < mBoundingBoxMesh->vertices.size(); ++i) mBoundingBoxMesh->indices.push_back(static_cast<unsigned int>(i));
        mBoundingBoxMesh->BuildGPU();
        lastMin = min; lastMax = max;
    }
    GLint old[2]; glGetIntegerv(GL_POLYGON_MODE, old);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_DEPTH_TEST);
    DrawMesh(*mBoundingBoxMesh, transform, nullptr, color);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, old[0]);
}

void Renderer::UpdateFogUniforms() const {
    if (mDefaultShader) {
        mDefaultShader->SetBool("uFogEnabled", mFog.enabled);
        mDefaultShader->SetVec3("uFogColor", Vec3(mFog.color.r, mFog.color.g, mFog.color.b));
        mDefaultShader->SetFloat("uFogStart", mFog.start);
        mDefaultShader->SetFloat("uFogEnd", mFog.end);
    }
}

// Skybox
Skybox::~Skybox() {
    if (mTextureID) glDeleteTextures(1, &mTextureID);
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
}
bool Skybox::Load(const std::array<std::string, 6>& faces) {
    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureID);
    int w,h,ch;
    stbi_set_flip_vertically_on_load(false);
    for (unsigned int i=0;i<6;i++) {
        unsigned char* data = stbi_load(faces.at(i).c_str(), &w,&h,&ch,0);
        if (data) {
            GLenum fmt = (ch==4)?GL_RGBA:GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
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
    float verts[] = {
        -1, 1,-1, -1,-1,-1, 1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
        -1,-1,1, -1,-1,-1, -1,1,-1, -1,1,-1, -1,1,1, -1,-1,1,
        1,-1,-1, 1,-1,1, 1,1,1, 1,1,1, 1,1,-1, 1,-1,-1,
        -1,-1,1, -1,1,1, 1,1,1, 1,1,1, 1,-1,1, -1,-1,1,
        -1,1,-1, 1,1,-1, 1,1,1, 1,1,1, -1,1,1, -1,1,-1,
        -1,-1,-1, -1,-1,1, 1,-1,-1, 1,-1,-1, -1,-1,1, 1,-1,1
    };
    glGenVertexArrays(1,&mVAO);
    glGenBuffers(1,&mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER,mVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),&verts,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    return true;
}
bool Skybox::LoadFromEquirectangular(const std::string& path) { (void)path; return false; }
void Skybox::Draw(const Camera& camera, Shader& shader) {
    if (!mTextureID) return;
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    shader.Bind();
    Mat4 view = Mat4(glm::mat3(camera.GetViewMatrix()));
    if (glm::length(mRotation)>0.001f) {
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
    glDrawArrays(GL_TRIANGLES,0,36);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}
void Renderer::DrawSkybox(const Camera& camera) {
    if (mSkybox.IsLoaded() && mSkyboxShader) mSkybox.Draw(camera, *mSkyboxShader);
}

} // namespace rpg
