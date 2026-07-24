#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Texture.h"
#include <algorithm>
#include <glad/gl.h>

namespace rpg {

Tileset::Tileset() = default;

Tileset::~Tileset() = default;

bool Tileset::Load(const std::string& texturePath, int tileWidth, int tileHeight, int spacing, int margin) {
    mTexture = std::make_unique<Texture>();
    if (!mTexture->LoadFromFile(texturePath)) {
        mTexture->CreateCheckerboard();
    } else {
        // Fix: Tileset soll NEAREST filtering nutzen, kein Mipmap Bleeding, sonst grauer Boden + Farb-Tiles vermischen
        // Setze Texture Parameter direkt via GL, da Texture::Load linear + mipmap setzt
        if (mTexture->GetID() != 0) {
            glBindTexture(GL_TEXTURE_2D, mTexture->GetID());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            // Kein Mipmap für Tileset – verhindert Farbvermischung über Tile-Grenzen
            glBindTexture(GL_TEXTURE_2D, 0);
        }
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
        // Heuristik: Markiere Tiles als solid wenn sie nicht Boden sind?
        // Für jetzt: Alle als nicht-solid, es sei denn TileId >= 8 (z.B. Wände) – kann via DB überschrieben werden
        // Nutzer will leeres neues Projekt, also keine festen Wände per Default.
        mTiles[i].solid = false;
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

// ---------------------------------------------------------------------------
// XP-Tileset-Flags (Paket 1, TODO_XP_PARITY.md)
// ---------------------------------------------------------------------------

void Tileset::SetTilesetData(const TilesetData& data) {
    mData = data;
    mHasData = true;
    // Legacy: TileInfo::solid aus passage befuellen, damit alte
    // Abfragen (Renderer, Editor-Vorschau) mitgehen.
    const int n = (int)mTiles.size();
    for (int i = 0; i < n; ++i) {
        mTiles[i].solid = (mData.GetPassage(i) != 0);
    }
}

int Tileset::GetPassage(int tileId) const {
    return mHasData ? mData.GetPassage(tileId) : 0;
}

int Tileset::GetPassage4Dir(int tileId) const {
    return mHasData ? mData.GetPassage4Dir(tileId) : 0;
}

int Tileset::GetPriority(int tileId) const {
    return mHasData ? mData.GetPriority(tileId) : 0;
}

int Tileset::GetMaxPriority() const {
    if (!mHasData) return 0;
    int mx = 0;
    for (int v : mData.priority)
        mx = std::max(mx, v);
    return mx;
}

int Tileset::GetBush(int tileId) const {
    return mHasData ? mData.GetBush(tileId) : 0;
}

int Tileset::GetCounter(int tileId) const {
    return mHasData ? mData.GetCounter(tileId) : 0;
}

int Tileset::GetTerrainTag(int tileId) const {
    return mHasData ? mData.GetTerrainTag(tileId) : 0;
}

bool Tileset::IsPassable(int tileId, int dirBit) const {
    // Ohne DB-Daten: Legacy-Verhalten (TileInfo::solid), sonst XP-Regeln.
    if (!mHasData) {
        const TileInfo* info = GetTileInfo(tileId);
        return !(info && info->solid);
    }
    if (mData.GetPassage(tileId) != 0) return false; // komplett blockiert
    if (dirBit != 0) {
        int d = mData.GetPassage4Dir(tileId);
        if (d != 0 && (d & dirBit) == 0) return false; // Richtung gesperrt
    }
    return true;
}

} // namespace rpg
