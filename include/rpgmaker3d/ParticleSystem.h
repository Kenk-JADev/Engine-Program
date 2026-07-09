#pragma once

#include "Types.h"
#include <vector>
#include <memory>

namespace rpg {

struct Particle {
    Vec3 position;
    Vec3 velocity;
    Color color;
    float life = 1.0f;
    float maxLife = 1.0f;
    float size = 1.0f;
};

class ParticleEmitter {
public:
    ParticleEmitter();

    void Emit(int count, const Vec3& origin, const Vec3& direction, float spread, float speed, float life, const Color& color);
    void Update(float dt);
    void Clear();

    const std::vector<Particle>& GetParticles() const { return mParticles; }

private:
    std::vector<Particle> mParticles;
};

} // namespace rpg
