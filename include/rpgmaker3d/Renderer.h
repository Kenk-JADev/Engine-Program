#pragma once

#include <memory>
#include "Types.h"
#include "Camera.h"

namespace rpg {

class Shader;
class Texture;
class Model;
class Mesh;

struct RenderCommand {
    Mesh* mesh = nullptr;
    Mat4 transform{1.0f};
    Texture* texture = nullptr;
    Color color{1.0f};
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

    void Submit(const RenderCommand& cmd);
    void Flush();

    void SetViewport(int x, int y, int width, int height);
    void EnableDepthTest(bool enable);
    void EnableWireframe(bool enable);

    Camera& GetCamera() { return mCamera; }

private:
    Camera mCamera;
    std::unique_ptr<Shader> mDefaultShader;
    std::unique_ptr<Texture> mDefaultTexture;
    std::vector<RenderCommand> mCommandQueue;
    Color mClearColor{0.1f, 0.1f, 0.15f, 1.0f};
};

} // namespace rpg
