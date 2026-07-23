#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Types.h"
#include "Database.h"   // TilesetData (XP-Flag-Tabellen)

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

    // ---- XP-Tileset-Flags (Paket 1, TODO_XP_PARITY.md) ----
    // Nach dem Laden der Grafik aufrufen: koppelt die Datenbank-Flag-Tabellen
    // (Durchgaengigkeit, 4-Richtung, Prioritaet, Busch, Tresen, Terrain-Tag)
    // an dieses Runtime-Tileset. Vorher: alles begehbar (Defaults).
    void SetTilesetData(const TilesetData& data);
    bool HasTilesetData() const { return mHasData; }

    int GetPassage(int tileId) const;     // 0=frei, 1=blockiert
    int GetPassage4Dir(int tileId) const; // Bits s. TilesetData::DirBit
    int GetPriority(int tileId) const;
    /// Groesste belegte Prioritaet (0..5) aus den DB-Daten, 0 ohne Daten.
    /// (Paket 6: RGSS z-Sortierung braucht den Maximalwert.)
    int GetMaxPriority() const;
    int GetBush(int tileId) const;
    int GetCounter(int tileId) const;
    int GetTerrainTag(int tileId) const;

    // XP-Regel: passage==1 -> nie begehbar; 4dir==0 -> alle Richtungen frei;
    // sonst muss das Richtungsbit gesetzt sein. dirBit=0 -> richtungslos.
    bool IsPassable(int tileId, int dirBit = 0) const;

private:
    std::unique_ptr<Texture> mTexture;
    int mTileWidth = 32;
    int mTileHeight = 32;
    int mSpacing = 0;
    int mMargin = 0;
    int mColumns = 0;
    int mRows = 0;
    std::vector<TileInfo> mTiles;
    TilesetData mData;     // DB-Flags (Kopie)
    bool mHasData = false; // true, sobald SetTilesetData aufgerufen wurde
};

} // namespace rpg
