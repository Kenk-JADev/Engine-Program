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
    mWidth = width;
    mHeight = height;
    for (auto& layer : mLayers) {
        layer.width = width;
        layer.height = height;
        layer.tiles.assign(width * height, -1);
    }
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
