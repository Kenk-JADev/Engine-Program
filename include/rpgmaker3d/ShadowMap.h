#pragma once

#include "Types.h"
#include <string>

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
    int mPreviousFBO = 0;
    Mat4 mLightSpaceMatrix{1.0f};
};

} // namespace rpg
