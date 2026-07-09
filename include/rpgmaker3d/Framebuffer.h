#pragma once

#include "Types.h"

typedef unsigned int GLuint;

namespace rpg {

class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();

    bool Create(int width, int height);
    void Resize(int width, int height);
    void Bind();
    void Unbind();
    void Delete();

    GLuint GetTextureID() const { return mColorTexture; }
    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }

private:
    GLuint mFBO = 0;
    GLuint mColorTexture = 0;
    GLuint mDepthRBO = 0;
    int mWidth = 0;
    int mHeight = 0;
};

} // namespace rpg
