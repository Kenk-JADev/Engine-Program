#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Renderer.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace rpg {

Map::Map() {
    Resize(20, 20);
}

Map::~Map() = default;

void Map::Resize(int width, int height) {
    // Inhalt erhalten (links-oben verankert, wie RPG Maker XP):
    // groessere Karte = leere Felder rechts/unten, kleinere = abgeschnitten.
    for (auto& layer : mLayers) {
        std::vector<int> old = std::move(layer.tiles);
        const int oldW = layer.width;
        const int oldH = layer.height;
        layer.width = width;
        layer.height = height;
        layer.tiles.assign((size_t)width * height, -1);
        if ((int)old.size() != oldW * oldH) continue; // Sicherheitsnetz
        const int cw = std::min(oldW, width);
        const int ch = std::min(oldH, height);
        for (int z = 0; z < ch; ++z)
            for (int x = 0; x < cw; ++x)
                layer.tiles[z * width + x] = old[z * oldW + x];
    }
    mWidth = width;
    mHeight = height;
    mDirty = true;
}

void Map::AddLayer(const std::string& name) {
    MapLayer layer;
    layer.name = name;
    layer.width = mWidth;
    layer.height = mHeight;
    layer.tiles.assign(mWidth * mHeight, -1);
    mLayers.push_back(layer);
    mDirty = true;
}

// ---------------------------------------------------------------------------
// PAKET 25: Prozedurale Standardkarte (spielbarer Fallback)
// Palette des Demo-Tilesets (assets/textures/tileset_demo.png, 8 Spalten x
// 6 Zeilen, ID = zeile*8 + spalte):
//   Spalte 0 = Gruentoene (Zeile 3/4/5 = hohes Gras, bush-Flag)
//   Spalte 1 = Erde (Weg), 2 = Stein (blockiert), 3 = Wasser (blockiert),
//   Spalte 4 = Sand (Ufer)
// ---------------------------------------------------------------------------
void Map::CreateFallback(int width, int height) {
    constexpr int kGrass     = 8;   // Spalte 0, Zeile 1
    constexpr int kTallGrass = 24;  // Spalte 0, Zeile 3 (Durchwiese)
    constexpr int kDirt      = 9;   // Spalte 1, Zeile 1
    constexpr int kSand      = 12;  // Spalte 4, Zeile 1
    constexpr int kStone     = 2;   // Spalte 2 (blockiert)
    constexpr int kWater     = 11;  // Spalte 3, Zeile 1 (blockiert)

    if (width < 6) width = 6;
    if (height < 6) height = 6;

    mLayers.clear();
    mWidth = width;
    mHeight = height;

    // --- Layer 0: Boden --------------------------------------------------
    AddLayer("Ground"); // benutzt die gesetzten mWidth/mHeight
    MapLayer& ground = mLayers.back();
    std::fill(ground.tiles.begin(), ground.tiles.end(), kGrass);

    auto gset = [&](int x, int z, int id) {
        if (x >= 0 && x < mWidth && z >= 0 && z < mHeight)
            ground.tiles[(size_t)z * (size_t)mWidth + (size_t)x] = id;
    };
    auto gget = [&](int x, int z) -> int {
        if (x < 0 || x >= mWidth || z < 0 || z >= mHeight) return -1;
        return ground.tiles[(size_t)z * (size_t)mWidth + (size_t)x];
    };

    // Feldweg-Kreuz (horizontal quer durch, vertikal nach Sueden)
    const int midX = mWidth / 2;
    const int midZ = mHeight / 2;
    for (int x = 1; x < mWidth - 1; ++x) gset(x, midZ, kDirt);
    for (int z = midZ; z < mHeight - 1; ++z) gset(midX, z, kDirt);

    // Teich (links oben) mit Sand-Ufer - Ellipse, Wasser blockiert
    {
        const int cx = std::max(3, mWidth / 5);
        const int cz = std::max(3, mHeight / 4);
        const int rx = std::max(2, mWidth / 9);
        const int rz = std::max(2, mHeight / 9);
        for (int z = cz - rz - 1; z <= cz + rz + 1; ++z) {
            for (int x = cx - rx - 1; x <= cx + rx + 1; ++x) {
                const float dx = static_cast<float>(x - cx) / static_cast<float>(rx);
                const float dz = static_cast<float>(z - cz) / static_cast<float>(rz);
                const float d = dx * dx + dz * dz;
                if (d <= 1.0f) gset(x, z, kWater);
                else if (d <= 1.45f) gset(x, z, kSand);
            }
        }
    }

    // Hohes Gras (Durchwiese-Patches; Begegnungsrate verdoppelt sich dort)
    for (int z = 0; z < mHeight; ++z) {
        for (int x = 0; x < mWidth; ++x) {
            const bool patch1 = (x >= mWidth * 7 / 10 && x < mWidth * 7 / 10 + 4 &&
                                 z >= 2 && z < 6);
            const bool patch2 = (x >= 2 && x < 6 &&
                                 z >= mHeight * 7 / 10 && z < mHeight * 7 / 10 + 3);
            if ((patch1 || patch2) && gget(x, z) == kGrass)
                gset(x, z, kTallGrass);
        }
    }

    // --- Layer 1: Hindernisse (knapp ueber dem Boden, kein Z-Fighting) ---
    AddLayer("Objects");
    MapLayer& objects = mLayers.back();
    objects.elevation = 0.02f;

    auto oset = [&](int x, int z, int id) {
        if (x >= 0 && x < mWidth && z >= 0 && z < mHeight)
            objects.tiles[(size_t)z * (size_t)mWidth + (size_t)x] = id;
    };

    // Mauer-Rand (blockiert - zusaetzlich zur Karten-Kanten-Klemme)
    for (int x = 0; x < mWidth; ++x) { oset(x, 0, kStone); oset(x, mHeight - 1, kStone); }
    for (int z = 1; z < mHeight - 1; ++z) { oset(0, z, kStone); oset(mWidth - 1, z, kStone); }

    // Felsen-Deko (deterministisch, nur auf freien Gras-Stellen)
    const int rockPos[5][2] = {
        { mWidth / 3,     mHeight * 2 / 3     },
        { mWidth / 3 + 1, mHeight * 2 / 3 + 1 },
        { mWidth * 3 / 5, 3                   },
        { mWidth - 5,     mHeight - 4         },
        { 3,              mHeight / 2         }
    };
    for (const auto& rp : rockPos) {
        const int x = rp[0], z = rp[1];
        if (x <= 0 || x >= mWidth - 1 || z <= 0 || z >= mHeight - 1) continue;
        if (gget(x, z) != kGrass) continue; // Weg/Teich/hohes Gras freilassen
        oset(x, z, kStone);
    }

    mDirty = true;
}

void Map::SetTile(int layer, int x, int z, int tileId) {
    if (layer < 0 || layer >= static_cast<int>(mLayers.size())) return;
    if (x < 0 || x >= mWidth || z < 0 || z >= mHeight) return;
    mLayers[layer].tiles[z * mWidth + x] = tileId;
    mDirty = true;
}

int Map::GetTile(int layer, int x, int z) const {
    if (layer < 0 || layer >= static_cast<int>(mLayers.size())) return -1;
    if (x < 0 || x >= mWidth || z < 0 || z >= mHeight) return -1;
    return mLayers[layer].tiles[z * mWidth + x];
}

void Map::BuildGeometry() {
    if (!mTileset) return;

    mMesh = std::make_unique<Mesh>();
    float tileW = 1.0f;
    float tileH = 1.0f;

    for (const auto& layer : mLayers) {
        if (!layer.visible) continue;
        for (int z = 0; z < mHeight; ++z) {
            for (int x = 0; x < mWidth; ++x) {
                int tileId = layer.tiles[z * mWidth + x];
                if (tileId < 0) continue;

                Vec4 uv = mTileset->GetTileUV(tileId);
                float ux = uv.x;
                float vy = uv.y;
                float uw = uv.z - uv.x;
                float vh = uv.w - uv.y;

                float px = (x - mWidth * 0.5f) * tileW;
                float pz = (z - mHeight * 0.5f) * tileH;
                float py = layer.elevation;

                unsigned int base = static_cast<unsigned int>(mMesh->vertices.size());

                mMesh->vertices.push_back({{px, py, pz + tileH}, {0, 1, 0}, {ux, vy + vh}});
                mMesh->vertices.push_back({{px + tileW, py, pz + tileH}, {0, 1, 0}, {ux + uw, vy + vh}});
                mMesh->vertices.push_back({{px + tileW, py, pz}, {0, 1, 0}, {ux + uw, vy}});
                mMesh->vertices.push_back({{px, py, pz}, {0, 1, 0}, {ux, vy}});

                mMesh->indices.push_back(base + 0);
                mMesh->indices.push_back(base + 1);
                mMesh->indices.push_back(base + 2);
                mMesh->indices.push_back(base + 2);
                mMesh->indices.push_back(base + 3);
                mMesh->indices.push_back(base + 0);
            }
        }
    }

    if (!mMesh->vertices.empty()) {
        mMesh->BuildGPU();
    }
    mDirty = false;
}

void Map::Render(Renderer& renderer) {
    if (mDirty) BuildGeometry();
    if (mMesh && !mMesh->vertices.empty()) {
        renderer.DrawMesh(*mMesh, Mat4(1.0f), mTileset ? mTileset->GetTexture() : nullptr);
    }
}

void Map::RenderDepth(Renderer& renderer) {
    if (mDirty) BuildGeometry();
    if (mMesh && !mMesh->vertices.empty()) {
        renderer.DrawMeshDepth(*mMesh, Mat4(1.0f));
    }
}

void Map::Save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    file.write(reinterpret_cast<const char*>(&mWidth), sizeof(mWidth));
    file.write(reinterpret_cast<const char*>(&mHeight), sizeof(mHeight));
    int layerCount = static_cast<int>(mLayers.size());
    file.write(reinterpret_cast<const char*>(&layerCount), sizeof(layerCount));

    for (const auto& layer : mLayers) {
        int nameLen = static_cast<int>(layer.name.size());
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        file.write(layer.name.data(), nameLen);
        file.write(reinterpret_cast<const char*>(&layer.elevation), sizeof(layer.elevation));
        file.write(reinterpret_cast<const char*>(layer.tiles.data()), layer.tiles.size() * sizeof(int));
    }
}

bool Map::Load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(&mWidth), sizeof(mWidth));
    file.read(reinterpret_cast<char*>(&mHeight), sizeof(mHeight));
    int layerCount = 0;
    file.read(reinterpret_cast<char*>(&layerCount), sizeof(layerCount));

    mLayers.clear();
    for (int i = 0; i < layerCount; ++i) {
        MapLayer layer;
        int nameLen = 0;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        layer.name.resize(nameLen);
        file.read(layer.name.data(), nameLen);
        file.read(reinterpret_cast<char*>(&layer.elevation), sizeof(layer.elevation));
        layer.width = mWidth;
        layer.height = mHeight;
        layer.tiles.resize(mWidth * mHeight);
        file.read(reinterpret_cast<char*>(layer.tiles.data()), layer.tiles.size() * sizeof(int));
        mLayers.push_back(layer);
    }

    mDirty = true;
    return true;
}

} // namespace rpg
