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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

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
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mWidth, mHeight);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // Reduce acne: polygon offset + front face culling handled in Renderer
}

void ShadowMap::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::Resize(int width, int height) {
    if (width == mWidth && height == mHeight) return;
    Create(width, height);
}

// ================= ShadowCubeMap =================

ShadowCubeMap::ShadowCubeMap() = default;
ShadowCubeMap::~ShadowCubeMap() { Delete(); }

bool ShadowCubeMap::Create(int size) {
    Delete();
    mSize = size;

    glGenFramebuffers(1, &mFBO);
    glGenTextures(1, &mDepthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mDepthCubemap);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mDepthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ShadowCubeMap Framebuffer incomplete" << std::endl;
        Delete();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void ShadowCubeMap::Delete() {
    if (mFBO) {
        glDeleteFramebuffers(1, &mFBO);
        mFBO = 0;
    }
    if (mDepthCubemap) {
        glDeleteTextures(1, &mDepthCubemap);
        mDepthCubemap = 0;
    }
    mSize = 0;
}

void ShadowCubeMap::Bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mSize, mSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void ShadowCubeMap::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowCubeMap::Resize(int size) {
    if (size == mSize) return;
    Create(size);
}

std::array<Mat4, 6> ShadowCubeMap::GetLightMatrices(const Vec3& lightPos, float nearPlane, float farPlane) const {
    std::array<Mat4, 6> matrices;
    Mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
    // +X, -X, +Y, -Y, +Z, -Z with corrected up vectors
    matrices[0] = proj * glm::lookAt(lightPos, lightPos + Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));
    matrices[1] = proj * glm::lookAt(lightPos, lightPos + Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));
    matrices[2] = proj * glm::lookAt(lightPos, lightPos + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
    matrices[3] = proj * glm::lookAt(lightPos, lightPos + Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    matrices[4] = proj * glm::lookAt(lightPos, lightPos + Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f));
    matrices[5] = proj * glm::lookAt(lightPos, lightPos + Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f));
    return matrices;
}

} // namespace rpg
