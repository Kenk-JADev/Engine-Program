#pragma once
// Minimal OpenGL 3.3 renderer backend for RmlUi, adapted to the engine's GL state.
// PoC for the ImGui -> RmlUi migration. Owns its own shader program; saves and
// restores the engine's GL state around UI rendering so nothing else is affected.

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>

namespace rpg {

class RmlUiRenderGL3 : public Rml::RenderInterface {
public:
    RmlUiRenderGL3();
    ~RmlUiRenderGL3() override;

    // Requires an active GL 3.3 context with glad loaded.
    bool Initialize();
    void Shutdown();

    // Screen size in pixels (for the pixel->NDC projection). Call on resize.
    void SetViewport(int width, int height);

    // Frame bracketing: saves engine GL state in Begin, restores in End.
    void BeginRender();
    void EndRender();

    // --- Rml::RenderInterface (required subset) ---
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* transform) override;

private:
    unsigned int CompileShaderProgram();
    unsigned int CreateWhiteTexture();
    unsigned int CreateTextureFromRGBA(const unsigned char* data, int w, int h);

    struct Geometry {
        unsigned int vao = 0, vbo = 0, ibo = 0;
        int numIndices = 0;
    };

    unsigned int mProgram = 0;
    int mLocProj = -1;
    int mLocTranslate = -1;
    int mLocTransform = -1;
    int mLocUseTransform = -1;
    int mLocTexture = -1;
    unsigned int mWhiteTexture = 0;

    int mWidth = 0;
    int mHeight = 0;

    Rml::Matrix4f mTransform;
    bool mTransformActive = false;

    bool mScissorEnabled = false;
    Rml::Rectanglei mScissor{};

    // Saved engine GL state (restored in EndRender)
    struct SavedState {
        int program = 0;
        int vao = 0;
        int arrayBuffer = 0;
        int elementBuffer = 0;
        int texture2D = 0;
        int activeTexture = 0;
        int blend = 0;
        int depthTest = 0;
        int scissorTest = 0;
        int cullFace = 0;
        int viewport[4] = {0, 0, 0, 0};
        int scissorBox[4] = {0, 0, 0, 0};
        int blendSrcRGB = 0, blendDstRGB = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
        int blendEqRGB = 0, blendEqAlpha = 0;
    };
    SavedState mSaved;
};

} // namespace rpg
