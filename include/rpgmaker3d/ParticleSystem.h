#pragma once

#include "Types.h"
#include <vector>
#include <memory>
#include <string>

namespace rpg {

enum class ParticleShape {
    Sphere = 0,
    Cone = 1,
    Box = 2
};

struct Particle {
    Vec3 position{0.0f};
    Vec3 velocity{0.0f};
    Color color{1.0f};
    Color startColor{1.0f};
    Color endColor{1.0f, 1.0f, 1.0f, 0.0f};
    float life = 1.0f;
    float maxLife = 1.0f;
    float size = 1.0f;
    float startSize = 0.15f;
    float endSize = 0.05f;
};

/// GPU-friendly CPU particle emitter with basic RPG VFX presets.
class ParticleEmitter {
public:
    ParticleEmitter();

    void Emit(int count, const Vec3& origin, const Vec3& direction, float spread, float speed, float life, const Color& color);
    void EmitBurst(const Vec3& origin);
    void Update(float dt);
    void Clear();

    const std::vector<Particle>& GetParticles() const { return mParticles; }
    int GetAliveCount() const { return static_cast<int>(mParticles.size()); }
    int GetMaxParticles() const { return mMaxParticles; }
    void SetMaxParticles(int maxCount) { mMaxParticles = std::max(1, maxCount); }

    // Simulation params
    bool useGravity = true;
    float gravity = 2.0f;
    bool loop = true;
    ParticleShape shape = ParticleShape::Cone;
    float shapeRadius = 0.1f;
    float drag = 0.0f; // 0..1 per second

    // Default burst params
    int defaultCount = 16;
    Vec3 defaultDirection{0.0f, 1.0f, 0.0f};
    float defaultSpread = 0.45f;
    float defaultSpeed = 2.5f;
    float defaultLife = 1.2f;
    Color defaultStartColor{1.0f, 0.55f, 0.1f, 1.0f};
    Color defaultEndColor{1.0f, 0.1f, 0.0f, 0.0f};
    float defaultStartSize = 0.18f;
    float defaultEndSize = 0.02f;

    /// Named presets: "fire", "smoke", "spark", "magic", "heal"
    void ApplyPreset(const std::string& name);

private:
    std::vector<Particle> mParticles;
    int mMaxParticles = 512;
};

} // namespace rpg
