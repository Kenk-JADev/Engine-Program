#pragma once

#include <string>
#include "Types.h"

namespace rpg {

struct PrefabData {
    std::string name = "Prefab";
    bool hasTransform = true;
    Vec3 position{0.0f};
    Vec3 rotation{0.0f};
    Vec3 scale{1.0f};
    bool hasModel = false;
    std::string modelType = "cube"; // cube, plane
    Color color{1.0f};
    bool hasLight = false;
    Color lightColor{1.0f};
    float lightIntensity = 1.0f;
};

class Prefab {
public:
    bool Save(const std::string& path, const PrefabData& data);
    bool Load(const std::string& path, PrefabData& data);

    static std::string GetPrefabDirectory();
};

} // namespace rpg
