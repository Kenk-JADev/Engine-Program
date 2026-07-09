#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Types.h"
#include "Tileset.h"

namespace rpg {

class Mesh;
class Renderer;

struct MapLayer {
    std::string name = "Layer";
    int width = 20;
    int height = 20;
    std::vector<int> tiles; // tile IDs, -1 = empty
    float elevation = 0.0f;
    bool visible = true;
};

class Map {
public:
    Map();
    ~Map();

    void Resize(int width, int height);
    void AddLayer(const std::string& name);
    void SetTile(int layer, int x, int z, int tileId);
    int GetTile(int layer, int x, int z) const;

    void SetTileset(std::shared_ptr<Tileset> tileset) { mTileset = tileset; mDirty = true; }
    std::shared_ptr<Tileset> GetTileset() const { return mTileset; }

    void BuildGeometry();
    void Render(Renderer& renderer);

    void Save(const std::string& path) const;
    bool Load(const std::string& path);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    const std::vector<MapLayer>& GetLayers() const { return mLayers; }
    std::vector<MapLayer>& GetLayers() { return mLayers; }

private:
    int mWidth = 20;
    int mHeight = 20;
    std::vector<MapLayer> mLayers;
    std::shared_ptr<Tileset> mTileset;
    std::unique_ptr<Mesh> mMesh;
    bool mDirty = true;
};

} // namespace rpg
