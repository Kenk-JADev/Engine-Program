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

    float tw = static_cast<float>(mTileWidth) / static_cast<float>(mTexture->GetWidth());
    float th = static_cast<float>(mTileHeight) / static_cast<float>(mTexture->GetHeight());
    float sx = static_cast<float>(mSpacing) / static_cast<float>(mTexture->GetWidth());
    float sy = static_cast<float>(mSpacing) / static_cast<float>(mTexture->GetHeight());

    float u = (tx * (tw + sx)) + (static_cast<float>(mMargin) / mTexture->GetWidth());
    float v = (ty * (th + sy)) + (static_cast<float>(mMargin) / mTexture->GetHeight());

    return Vec4(u, v, u + tw, v + th);
}

const TileInfo* Tileset::GetTileInfo(int tileId) const {
    if (tileId < 0 || tileId >= static_cast<int>(mTiles.size())) return nullptr;
    return &mTiles[tileId];
}

} // namespace rpg
