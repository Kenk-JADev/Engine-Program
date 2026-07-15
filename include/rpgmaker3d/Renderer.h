#pragma once

#include <memory>
#include <array>
#include <vector>
#include "Types.h"
#include "Camera.h"
#include "Material.h"

namespace rpg {

struct Particle;

class Shader;
class Texture;
class Model;
class Mesh;
class ShadowMap;
class ShadowCubeMap;
class Scene;

struct RenderCommand {
    Mesh* mesh = nullptr;
    Mat4 transform{1.0f};
    Texture* texture = nullptr;
    Color color{1.0f};
};

// Fog settings (disabled by default – avoids washed-out grey scene)
struct FogSettings {
    bool enabled = false;
    Color color = Color(0.55f, 0.60f, 0.70f, 1.0f);
    float start = 25.0f;
    float end = 180.0f;
    float density = 0.01f;  // for exponential fog
};

// Skybox
class Skybox {
public:
    Skybox() = default;
    ~Skybox();
    bool Load(const std::array<std::string, 6>& faces);  // right, left, top, bottom, front, back
    bool LoadFromEquirectangular(const std::string& path);  // HDR panorama
    void Draw(const Camera& camera, Shader& shader);
    bool IsLoaded() const { return mTextureID != 0; }
    void SetRotation(const Vec3& rot) { mRotation = rot; }
    const Vec3& GetRotation() const { return mRotation; }
    Vec3& GetRotation() { return mRotation; }
    // Accessors for Renderer shutdown
    unsigned int& GetTextureID() { return mTextureID; }
    const unsigned int& GetTextureID() const { return mTextureID; }
    void SetTextureID(unsigned int id) { mTextureID = id; }
    unsigned int& GetVAO() { return mVAO; }
    const unsigned int& GetVAO() const { return mVAO; }
    void SetVAO(unsigned int vao) { mVAO = vao; }
    unsigned int& GetVBO() { return mVBO; }
    const unsigned int& GetVBO() const { return mVBO; }
    void SetVBO(unsigned int vbo) { mVBO = vbo; }

private:
    unsigned int mTextureID = 0;
    unsigned int mVAO = 0, mVBO = 0;
    Vec3 mRotation{0.0f};
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Initialize();
    void Shutdown();

    void BeginFrame(const Camera& camera);
    void EndFrame();

    void SetClearColor(const Color& color);
    void Clear();

    void DrawMesh(const Mesh& mesh, const Mat4& transform, Texture* texture = nullptr, const Color& color = Color(1.0f));
    void DrawModel(const Model& model, const Mat4& transform, Texture* texture = nullptr);
    void DrawMeshWithMaterial(const Mesh& mesh, const Mat4& transform, const Material& material);
    void DrawParticles(const std::vector<Particle>& particles);
    void DrawBoundingBox(const Vec3& min, const Vec3& max, const Mat4& transform, const Color& color);
    /// Draw line-list mesh (e.g. editor grid) without triangle fill
    void DrawGrid(const Mesh& mesh, const Mat4& transform, const Color& color = Color(0.4f, 0.4f, 0.45f, 0.6f));

    void Submit(const RenderCommand& cmd);
    void Flush();

    void SetViewport(int x, int y, int width, int height);
    void EnableDepthTest(bool enable);
    void EnableWireframe(bool enable);
    bool IsWireframeEnabled() const { return mWireframeEnabled; }

    Camera& GetCamera() { return mCamera; }
    Shader& GetShader() { return *mDefaultShader; }

    void UpdateLighting();
    void SetLightDir(const Vec3& dir);
    void SetAmbient(float ambient);

    // Fog control
    void SetFog(const FogSettings& fog) { mFog = fog; }
    const FogSettings& GetFog() const { return mFog; }
    FogSettings& GetFog() { return mFog; }
    void SetFogEnabled(bool enabled) { mFog.enabled = enabled; }
    void SetFogColor(const Color& color) { mFog.color = color; }
    void SetFogRange(float start, float end) { mFog.start = start; mFog.end = end; }

    // Skybox
    Skybox& GetSkybox() { return mSkybox; }
    const Skybox& GetSkybox() const { return mSkybox; }
    void DrawSkybox(const Camera& camera);

    // Shadows – directional
    bool InitShadowSystem(int mapSize = 2048);
    void ShutdownShadowSystem();
    void BeginShadowPass();
    void EndShadowPass();
    void DrawMeshDepth(const Mesh& mesh, const Mat4& transform);
    void DrawModelDepth(const Model& model, const Mat4& transform);
    Mat4 CalculateLightSpaceMatrix(float orthoSize = 30.0f, float nearPlane = 1.0f, float farPlane = 60.0f);
    unsigned int GetShadowMapTexture() const;
    const Mat4& GetLightSpaceMatrix() const { return mLightSpaceMatrix; }
    bool IsShadowsEnabled() const { return mShadowsEnabled; }
    void SetShadowsEnabled(bool enabled) { mShadowsEnabled = enabled; }
    Shader* GetShadowShader() { return mShadowShader.get(); }

    // Point light cubemap shadows
    bool InitPointShadowSystem(int cubeSize = 1024);
    void ShutdownPointShadowSystem();
    void RenderPointShadows(Scene& scene);
    bool IsPointShadowsEnabled() const { return mPointShadowsEnabled; }
    void SetPointShadowsEnabled(bool v) { mPointShadowsEnabled = v; }
    unsigned int GetPointShadowCubemap(int index) const;
    int GetPointShadowCount() const { return static_cast<int>(mPointShadowMaps.size()); }

private:
    void UpdateFogUniforms() const;
    void UpdateShadowUniforms() const;
    void UpdatePointShadowUniforms() const;

    Camera mCamera;
    std::unique_ptr<Shader> mDefaultShader;
    std::unique_ptr<Shader> mSkyboxShader;
    std::unique_ptr<Shader> mShadowShader;
    std::unique_ptr<Shader> mPointShadowShader;
    std::unique_ptr<Texture> mDefaultTexture;
    std::unique_ptr<Mesh> mParticleMesh;
    std::unique_ptr<Mesh> mBoundingBoxMesh;
    std::unique_ptr<ShadowMap> mShadowMap;
    std::vector<std::unique_ptr<ShadowCubeMap>> mPointShadowMaps;
    std::vector<RenderCommand> mCommandQueue;
    Color mClearColor{0.12f, 0.13f, 0.16f, 1.0f};
    bool mWireframeEnabled = false;
    Mat4 mLightSpaceMatrix{1.0f};
    bool mShadowsEnabled = true;
    bool mPointShadowsEnabled = true;
    int mShadowMapSize = 2048;
    int mPointShadowSize = 1024;
    
    FogSettings mFog;
    Skybox mSkybox;
};

} // namespace rpg
