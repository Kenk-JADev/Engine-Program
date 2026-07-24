#include "rpgmaker3d/Texture.h"
#include <glad/gl.h>
#include <stb_image.h>
#include <iostream>

namespace rpg {

Texture::Texture() = default;

Texture::~Texture() {
    Delete();
}

bool Texture::LoadFromFile(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &mWidth, &mHeight, &mChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return false;
    }

    if (mTextureID != 0) Delete();
    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    GLint format = mChannels == 4 ? GL_RGBA : (mChannels == 3 ? GL_RGB : GL_RED);
    glTexImage2D(GL_TEXTURE_2D, 0, format, mWidth, mHeight, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    mPath = path;
    return true;
}

bool Texture::ReadPixelsRGBA(std::vector<unsigned char>& outPixels) const {
    if (mTextureID == 0 || mWidth <= 0 || mHeight <= 0) return false;
    outPixels.resize(static_cast<size_t>(mWidth) * static_cast<size_t>(mHeight) * 4u);
    glBindTexture(GL_TEXTURE_2D, mTextureID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, outPixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool Texture::CreateFromRGBA(int width, int height, const unsigned char* rgbaPixels) {
    if (width <= 0 || height <= 0 || !rgbaPixels) return false;
    if (mTextureID != 0) Delete();
    mWidth = width;
    mHeight = height;
    mChannels = 4;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    mPath.clear(); // CPU-generiert, kein Dateipfad
    return true;
}

void Texture::CreateDefault() {
    CreateCheckerboard();
}

void Texture::CreateCheckerboard() {
    const int size = 64;
    unsigned char data[size * size * 3];
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool c = ((x / 8) + (y / 8)) % 2 == 0;
            unsigned char v = c ? 200 : 80;
            int i = (y * size + x) * 3;
            data[i + 0] = v;
            data[i + 1] = v;
            data[i + 2] = v;
        }
    }
    mWidth = size;
    mHeight = size;
    mChannels = 3;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, mTextureID);
}

void Texture::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Delete() {
    if (mTextureID != 0) {
        glDeleteTextures(1, &mTextureID);
        mTextureID = 0;
    }
}

} // namespace rpg
