#include "rpgmaker3d/ParticleSystem.h"
#include <algorithm>
#include <glm/gtc/random.hpp>

namespace rpg {

ParticleEmitter::ParticleEmitter() = default;

void ParticleEmitter::Emit(int count, const Vec3& origin, const Vec3& direction, float spread, float speed, float life, const Color& color) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = origin;
        Vec3 dir = glm::normalize(direction + glm::sphericalRand(spread));
        p.velocity = dir * speed * (0.8f + glm::linearRand(0.0f, 0.4f));
        p.color = color;
        p.maxLife = life * (0.8f + glm::linearRand(0.0f, 0.4f));
        p.life = p.maxLife;
        p.size = 0.1f + glm::linearRand(0.0f, 0.1f);
        mParticles.push_back(p);
    }
}

void ParticleEmitter::Update(float dt) {
    for (auto& p : mParticles) {
        p.velocity.y -= 2.0f * dt; // gravity
        p.position += p.velocity * dt;
        p.life -= dt;
        float t = p.life / p.maxLife;
        p.color.a = t;
    }

    mParticles.erase(
        std::remove_if(mParticles.begin(), mParticles.end(),
            [](const Particle& p) { return p.life <= 0.0f; }),
        mParticles.end());
}

void ParticleEmitter::Clear() {
    mParticles.clear();
}

} // namespace rpg
