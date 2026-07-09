#pragma once

#include "Types.h"
#include <vector>

namespace rpg {

struct DirectionalLight {
    Vec3 direction = Vec3(0.3f, -1.0f, 0.5f);
    Color color = Color(1.0f, 1.0f, 0.95f, 1.0f);
    float intensity = 1.0f;
};

struct AmbientLight {
    Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    float intensity = 0.35f;
};

class Lighting {
public:
    static Lighting& Get();

    DirectionalLight& GetDirectionalLight() { return mDirectional; }
    AmbientLight& GetAmbient() { return mAmbient; }

    Vec3 GetEffectiveLightDir() const;
    float GetEffectiveAmbient() const;

private:
    Lighting() = default;
    DirectionalLight mDirectional;
    AmbientLight mAmbient;
};

} // namespace rpg
