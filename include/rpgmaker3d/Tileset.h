#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Types.h"

namespace rpg {

class Texture;

struct TileInfo {
    int id = 0;
    int tileX = 0;
    int tileY = 0;
    bool solid = false;
    float height = 1.0f;
};

class Tileset {
public:
    Tileset();
    ~Tileset();

    bool Load(const std::string& texturePath, int tileWidth, int tileHeight, int spacing = 0, int margin = 0);
    void SetTileInfo(int tileId, const TileInfo& info);

    int GetTileWidth() const { return mTileWidth; }
    int GetTileHeight() const { return mTileHeight; }
    int GetColumns() const { return mColumns; }
    int GetRows() const { return mRows; }

    Vec4 GetTileUV(int tileId) const;
    Texture* GetTexture() const { return mTexture.get(); }

    const TileInfo* GetTileInfo(int tileId) const;

private:
    std::unique_ptr<Texture> mTexture;
    int mTileWidth = 32;
    int mTileHeight = 32;
    int mSpacing = 0;
    int mMargin = 0;
    int mColumns = 0;
    int mRows = 0;
    std::vector<TileInfo> mTiles;
};

} // namespace rpg
