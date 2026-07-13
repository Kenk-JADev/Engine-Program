#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Texture.h"

namespace rpg {

Tileset::Tileset() = default;

Tileset::~Tileset() = default;

bool Tileset::Load(const std::string& texturePath, int tileWidth, int tileHeight, int spacing, int margin) {
    mTexture = std::make_unique<Texture>();
    if (!mTexture->LoadFromFile(texturePath)) {
        mTexture->CreateCheckerboard();
    }

    mTileWidth = tileWidth;
    mTileHeight = tileHeight;
    mSpacing = spacing;
    mMargin = margin;

    int usableW = mTexture->GetWidth() - 2 * margin;
    int usableH = mTexture->GetHeight() - 2 * margin;
    mColumns = usableW / (tileWidth + spacing);
    mRows = usableH / (tileHeight + spacing);

    int count = mColumns * mRows;
    mTiles.resize(count);
    for (int i = 0; i < count; ++i) {
        mTiles[i].id = i;
        mTiles[i].tileX = i % mColumns;
        mTiles[i].tileY = i / mColumns;
    }
    return true;
}

void Tileset::SetTileInfo(int tileId, const TileInfo& info) {
    if (tileId >= 0 && tileId < static_cast<int>(mTiles.size())) {
        mTiles[tileId] = info;
        mTiles[tileId].id = tileId;
    }
}

Vec4 Tileset::GetTileUV(int tileId) const {
    if (tileId < 0 || tileId >= static_cast<int>(mTiles.size()) || mColumns == 0 || mRows == 0) {
        return Vec4(0, 0, 1, 1);
    }

    int tx = tileId % mColumns;
    int ty = tileId / mColumns;

    float texW = static_cast<float>(mTexture->GetWidth());
    float texH = static_cast<float>(mTexture->GetHeight());
    
    float tw = static_cast<float>(mTileWidth) / texW;
    float th = static_cast<float>(mTileHeight) / texH;
    float sx = static_cast<float>(mSpacing) / texW;
    float sy = static_cast<float>(mSpacing) / texH;
    float mx = static_cast<float>(mMargin) / texW;
    float my = static_cast<float>(mMargin) / texH;

    float u = (tx * (tw + sx)) + mx;
    float v = (ty * (th + sy)) + my;

    // Add small padding to prevent texture bleeding (0.5 pixel inset)
    float paddingU = 0.5f / texW;
    float paddingV = 0.5f / texH;
    
    return Vec4(u + paddingU, v + paddingV, u + tw - paddingU, v + th - paddingV);
}

const TileInfo* Tileset::GetTileInfo(int tileId) const {
    if (tileId < 0 || tileId >= static_cast<int>(mTiles.size())) return nullptr;
    return &mTiles[tileId];
}

} // namespace rpg
