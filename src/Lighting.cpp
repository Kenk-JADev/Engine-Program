#include "rpgmaker3d/Lighting.h"

namespace rpg {

Lighting& Lighting::Get() {
    static Lighting instance;
    return instance;
}

Vec3 Lighting::GetEffectiveLightDir() const {
    return glm::normalize(mDirectional.direction);
}

float Lighting::GetEffectiveAmbient() const {
    return mAmbient.intensity;
}

} // namespace rpg
