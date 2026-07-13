#pragma once

#include "Types.h"
#include <vector>
#include <string>

namespace rpg {

enum class LightType {
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct DirectionalLight {
    Vec3 direction = Vec3(0.35f, -1.0f, 0.45f);
    Color color = Color(1.0f, 0.98f, 0.92f, 1.0f);
    float intensity = 1.15f;
    bool enabled = true;
};

struct AmbientLight {
    Color color = Color(0.55f, 0.60f, 0.70f, 1.0f);
    float intensity = 0.28f;
};

struct PointLight {
    bool enabled = false;
    Vec3 position{0.0f, 3.0f, 0.0f};
    Color color{1.0f, 0.9f, 0.7f, 1.0f};
    float intensity = 1.0f;
    float range = 12.0f;
};

struct SpotLight {
    bool enabled = false;
    Vec3 position{0.0f, 5.0f, 0.0f};
    Vec3 direction{0.0f, -1.0f, 0.0f};
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    float intensity = 1.5f;
    float range = 20.0f;
    float innerConeDeg = 15.0f;
    float outerConeDeg = 30.0f;
};

/// Global lighting singleton used by the forward renderer.
class Lighting {
public:
    static Lighting& Get();

    DirectionalLight& GetDirectionalLight() { return mDirectional; }
    const DirectionalLight& GetDirectionalLight() const { return mDirectional; }
    AmbientLight& GetAmbient() { return mAmbient; }
    const AmbientLight& GetAmbient() const { return mAmbient; }

    PointLight& GetPointLight(size_t index);
    const PointLight& GetPointLight(size_t index) const;
    size_t GetPointLightCount() const { return mPointLights.size(); }
    PointLight& AddPointLight();
    void ClearPointLights();

    SpotLight& GetSpotLight() { return mSpot; }
    const SpotLight& GetSpotLight() const { return mSpot; }

    Vec3 GetEffectiveLightDir() const;
    float GetEffectiveAmbient() const;

    /// Apply a preset lighting setup (editor convenience).
    void ApplyPreset(const std::string& name);

    /// Day/night cycle helpers (0..24 hours)
    void SetTimeOfDay(float hours);
    float GetTimeOfDay() const { return mTimeOfDay; }
    void UpdateTimeOfDay(float dt, float speed);

private:
    Lighting() = default;
    DirectionalLight mDirectional;
    AmbientLight mAmbient;
    std::vector<PointLight> mPointLights;
    SpotLight mSpot;
    float mTimeOfDay = 12.0f;
};

} // namespace rpg
