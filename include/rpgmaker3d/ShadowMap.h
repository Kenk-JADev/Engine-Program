#pragma once

#include "Types.h"
#include <string>
#include <array>

namespace rpg {

class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    bool Create(int width, int height);
    void Delete();
    void Bind();
    void Unbind();

    void Resize(int width, int height);

    unsigned int GetDepthTextureID() const { return mDepthTexture; }
    unsigned int GetFBO() const { return mFBO; }
    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }

    void SetLightSpaceMatrix(const Mat4& mat) { mLightSpaceMatrix = mat; }
    const Mat4& GetLightSpaceMatrix() const { return mLightSpaceMatrix; }

    bool IsValid() const { return mFBO != 0 && mDepthTexture != 0; }

private:
    unsigned int mFBO = 0;
    unsigned int mDepthTexture = 0;
    int mWidth = 0;
    int mHeight = 0;
    Mat4 mLightSpaceMatrix{1.0f};
};

// Cubemap shadow for point lights
class ShadowCubeMap {
public:
    ShadowCubeMap();
    ~ShadowCubeMap();

    bool Create(int size = 1024);
    void Delete();
    void Bind();
    void Unbind();
    void Resize(int size);

    unsigned int GetDepthCubemapID() const { return mDepthCubemap; }
    unsigned int GetFBO() const { return mFBO; }
    int GetSize() const { return mSize; }
    bool IsValid() const { return mFBO != 0 && mDepthCubemap != 0; }

    // Per-face view-projection matrices (6)
    std::array<Mat4, 6> GetLightMatrices(const Vec3& lightPos, float nearPlane, float farPlane) const;

private:
    unsigned int mFBO = 0;
    unsigned int mDepthCubemap = 0;
    int mSize = 0;
};

} // namespace rpg
