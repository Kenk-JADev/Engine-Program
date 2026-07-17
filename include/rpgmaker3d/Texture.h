#pragma once

#include <string>
#include <vector>
#include "Types.h"

typedef unsigned int GLuint;

namespace rpg {

class Texture {
public:
    Texture();
    ~Texture();

    bool LoadFromFile(const std::string& path);
    void CreateDefault();
    void CreateCheckerboard();
    void Bind(unsigned int slot = 0) const;
    void Unbind() const;
    void Delete();

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    GLuint GetID() const { return mTextureID; }
    const std::string& GetPath() const { return mPath; }

    // CPU-Pixel lesen (RGBA8). Benoetigt current GL-Kontext.
    bool ReadPixelsRGBA(std::vector<unsigned char>& outPixels) const;

private:
    GLuint mTextureID = 0;
    int mWidth = 0;
    int mHeight = 0;
    int mChannels = 0;
    std::string mPath;
};

} // namespace rpg
