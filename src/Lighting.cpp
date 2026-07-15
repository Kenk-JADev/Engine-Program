#include "rpgmaker3d/Lighting.h"
#include <algorithm>
#include <cmath>

namespace rpg {

Lighting& Lighting::Get() {
    static Lighting instance;
    return instance;
}

PointLight& Lighting::GetPointLight(size_t index) {
    if (index >= mPointLights.size()) {
        static PointLight dummy;
        return dummy;
    }
    return mPointLights[index];
}

const PointLight& Lighting::GetPointLight(size_t index) const {
    if (index >= mPointLights.size()) {
        static PointLight dummy;
        return dummy;
    }
    return mPointLights[index];
}

PointLight& Lighting::AddPointLight() {
    mPointLights.emplace_back();
    mPointLights.back().enabled = true;
    return mPointLights.back();
}

void Lighting::ClearPointLights() {
    mPointLights.clear();
}

Vec3 Lighting::GetEffectiveLightDir() const {
    if (!mDirectional.enabled) return Vec3(0.0f, -1.0f, 0.0f);
    Vec3 d = mDirectional.direction;
    float len = glm::length(d);
    if (len < 1e-5f) return Vec3(0.0f, -1.0f, 0.0f);
    return d / len;
}

float Lighting::GetEffectiveAmbient() const {
    return mAmbient.intensity;
}

void Lighting::ApplyPreset(const std::string& name) {
    if (name == "noon" || name == "Mittag") {
        mDirectional.direction = Vec3(0.2f, -1.0f, 0.15f);
        mDirectional.color = Color(1.0f, 0.98f, 0.92f, 1.0f);
        mDirectional.intensity = 1.3f;
        mAmbient.color = Color(0.55f, 0.62f, 0.75f, 1.0f);
        mAmbient.intensity = 0.32f;
        mTimeOfDay = 12.0f;
    } else if (name == "sunset" || name == "Abend") {
        mDirectional.direction = Vec3(0.85f, -0.25f, 0.15f);
        mDirectional.color = Color(1.0f, 0.55f, 0.25f, 1.0f);
        mDirectional.intensity = 1.1f;
        mAmbient.color = Color(0.35f, 0.25f, 0.35f, 1.0f);
        mAmbient.intensity = 0.22f;
        mTimeOfDay = 18.5f;
    } else if (name == "night" || name == "Nacht") {
        mDirectional.direction = Vec3(0.1f, -1.0f, 0.3f);
        mDirectional.color = Color(0.45f, 0.55f, 0.85f, 1.0f);
        mDirectional.intensity = 0.25f;
        mAmbient.color = Color(0.12f, 0.14f, 0.22f, 1.0f);
        mAmbient.intensity = 0.12f;
        mTimeOfDay = 1.0f;
    } else if (name == "overcast" || name == "Bewölkt") {
        mDirectional.direction = Vec3(0.2f, -1.0f, 0.1f);
        mDirectional.color = Color(0.85f, 0.88f, 0.92f, 1.0f);
        mDirectional.intensity = 0.55f;
        mAmbient.color = Color(0.70f, 0.72f, 0.75f, 1.0f);
        mAmbient.intensity = 0.45f;
        mTimeOfDay = 14.0f;
    } else {
        // default studio
        mDirectional.direction = Vec3(0.35f, -1.0f, 0.45f);
        mDirectional.color = Color(1.0f, 0.98f, 0.92f, 1.0f);
        mDirectional.intensity = 1.15f;
        mAmbient.color = Color(0.55f, 0.60f, 0.70f, 1.0f);
        mAmbient.intensity = 0.28f;
        mTimeOfDay = 12.0f;
    }
}

void Lighting::SetTimeOfDay(float hours) {
    mTimeOfDay = std::fmod(std::max(0.0f, hours), 24.0f);
    // Map hour to sun direction and warm/cool colors
    float t = mTimeOfDay / 24.0f; // 0..1
    float elev = std::sin((t - 0.25f) * 6.2831853f); // night negative
    float azim = (t - 0.25f) * 6.2831853f;
    mDirectional.direction = glm::normalize(Vec3(std::cos(azim), -std::max(0.05f, elev), std::sin(azim) * 0.5f));

    if (elev > 0.2f) {
        // Day
        mDirectional.color = Color(1.0f, 0.98f, 0.92f, 1.0f);
        mDirectional.intensity = 0.6f + elev * 0.8f;
        mAmbient.color = Color(0.55f, 0.62f, 0.75f, 1.0f);
        mAmbient.intensity = 0.2f + elev * 0.2f;
    } else if (elev > 0.0f) {
        // Golden hour
        mDirectional.color = Color(1.0f, 0.6f, 0.3f, 1.0f);
        mDirectional.intensity = 0.5f + elev * 1.5f;
        mAmbient.color = Color(0.4f, 0.28f, 0.35f, 1.0f);
        mAmbient.intensity = 0.18f;
    } else {
        // Night
        mDirectional.color = Color(0.4f, 0.5f, 0.85f, 1.0f);
        mDirectional.intensity = 0.15f;
        mAmbient.color = Color(0.1f, 0.12f, 0.2f, 1.0f);
        mAmbient.intensity = 0.1f;
    }
}

void Lighting::UpdateTimeOfDay(float dt, float speed) {
    if (speed <= 0.0f) return;
    SetTimeOfDay(mTimeOfDay + dt * speed);
}

} // namespace rpg
