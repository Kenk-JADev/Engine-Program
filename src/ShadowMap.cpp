#include "rpgmaker3d/ShadowMap.h"
#include <glad/gl.h>
#include <iostream>

namespace rpg {

ShadowMap::ShadowMap() = default;
ShadowMap::~ShadowMap() { Delete(); }

bool ShadowMap::Create(int width, int height) {
    Delete();
    mWidth = width;
    mHeight = height;

    glGenFramebuffers(1, &mFBO);
    glGenTextures(1, &mDepthTexture);

    glBindTexture(GL_TEXTURE_2D, mDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // For PCF, we could use linear and compare mode, but keep nearest for now
    // Enable hardware PCF if desired:
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ShadowMap Framebuffer incomplete" << std::endl;
        Delete();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void ShadowMap::Delete() {
    if (mFBO) {
        glDeleteFramebuffers(1, &mFBO);
        mFBO = 0;
    }
    if (mDepthTexture) {
        glDeleteTextures(1, &mDepthTexture);
        mDepthTexture = 0;
    }
    mWidth = 0;
    mHeight = 0;
}

void ShadowMap::Bind() {
    // Save previously bound framebuffer so we can restore it in Unbind()
    // This is critical: without this, the editor's scene framebuffer gets
    // silently unbound and all subsequent scene rendering goes to the
    // default framebuffer (screen) instead of the scene framebuffer.
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mPreviousFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mWidth, mHeight);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void ShadowMap::Unbind() {
    // Restore the previously bound framebuffer instead of unconditionally
    // binding 0 (the default/screen framebuffer). This ensures the editor's
    // scene framebuffer is properly restored after the shadow pass.
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(mPreviousFBO));
}

void ShadowMap::Resize(int width, int height) {
    if (width == mWidth && height == mHeight) return;
    Create(width, height);
}

} // namespace rpg
