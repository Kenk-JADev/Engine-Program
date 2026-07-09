#pragma once

#include "Types.h"
#include <memory>
#include <string>

namespace rpg {

class Texture;

struct Material {
    Color diffuse = Color(1.0f);
    Color emissive = Color(0.0f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    float alpha = 1.0f;
    bool transparent = false;
    bool wireframe = false;
    std::string texturePath;
    std::shared_ptr<Texture> texture;

    void BindDefaults();
};

} // namespace rpg
