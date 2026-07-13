#include "rpgmaker3d/ParticleSystem.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/random.hpp>

namespace rpg {

ParticleEmitter::ParticleEmitter() = default;

void ParticleEmitter::ApplyPreset(const std::string& name) {
    if (name == "fire") {
        useGravity = true; gravity = 0.4f;
        defaultDirection = Vec3(0, 1, 0);
        defaultSpread = 0.35f; defaultSpeed = 1.8f; defaultLife = 0.9f;
        defaultStartColor = Color(1.0f, 0.55f, 0.05f, 1.0f);
        defaultEndColor = Color(0.2f, 0.02f, 0.0f, 0.0f);
        defaultStartSize = 0.22f; defaultEndSize = 0.04f;
        defaultCount = 24;
    } else if (name == "smoke") {
        useGravity = true; gravity = -0.3f; // rises
        defaultDirection = Vec3(0, 1, 0);
        defaultSpread = 0.5f; defaultSpeed = 0.8f; defaultLife = 2.5f;
        defaultStartColor = Color(0.5f, 0.5f, 0.5f, 0.7f);
        defaultEndColor = Color(0.3f, 0.3f, 0.3f, 0.0f);
        defaultStartSize = 0.25f; defaultEndSize = 0.7f;
        defaultCount = 12;
        drag = 0.4f;
    } else if (name == "spark") {
        useGravity = true; gravity = 6.0f;
        defaultDirection = Vec3(0, 1, 0);
        defaultSpread = 0.9f; defaultSpeed = 5.0f; defaultLife = 0.6f;
        defaultStartColor = Color(1.0f, 0.9f, 0.4f, 1.0f);
        defaultEndColor = Color(1.0f, 0.2f, 0.0f, 0.0f);
        defaultStartSize = 0.08f; defaultEndSize = 0.01f;
        defaultCount = 32;
    } else if (name == "magic") {
        useGravity = false; gravity = 0.0f;
        defaultDirection = Vec3(0, 1, 0);
        defaultSpread = 1.0f; defaultSpeed = 1.2f; defaultLife = 1.5f;
        defaultStartColor = Color(0.5f, 0.3f, 1.0f, 1.0f);
        defaultEndColor = Color(0.1f, 0.8f, 1.0f, 0.0f);
        defaultStartSize = 0.15f; defaultEndSize = 0.0f;
        defaultCount = 20;
    } else if (name == "heal") {
        useGravity = true; gravity = -1.2f;
        defaultDirection = Vec3(0, 1, 0);
        defaultSpread = 0.25f; defaultSpeed = 1.0f; defaultLife = 1.3f;
        defaultStartColor = Color(0.3f, 1.0f, 0.45f, 1.0f);
        defaultEndColor = Color(0.9f, 1.0f, 0.9f, 0.0f);
        defaultStartSize = 0.12f; defaultEndSize = 0.02f;
        defaultCount = 18;
    }
}

void ParticleEmitter::Emit(int count, const Vec3& origin, const Vec3& direction, float spread, float speed, float life, const Color& color) {
    for (int i = 0; i < count; ++i) {
        if (static_cast<int>(mParticles.size()) >= mMaxParticles) break;

        Particle p;
        // Spawn offset by shape
        if (shape == ParticleShape::Sphere) {
            p.position = origin + glm::sphericalRand(shapeRadius);
        } else if (shape == ParticleShape::Box) {
            p.position = origin + Vec3(
                glm::linearRand(-shapeRadius, shapeRadius),
                glm::linearRand(-shapeRadius, shapeRadius),
                glm::linearRand(-shapeRadius, shapeRadius));
        } else {
            p.position = origin + Vec3(
                glm::linearRand(-shapeRadius, shapeRadius),
                0.0f,
                glm::linearRand(-shapeRadius, shapeRadius));
        }

        Vec3 dir = direction;
        if (glm::length(dir) < 1e-5f) dir = Vec3(0, 1, 0);
        dir = glm::normalize(dir + glm::sphericalRand(spread));
        p.velocity = dir * speed * (0.75f + glm::linearRand(0.0f, 0.5f));

        p.startColor = color;
        p.endColor = Color(color.r, color.g, color.b, 0.0f);
        p.color = color;
        p.maxLife = life * (0.8f + glm::linearRand(0.0f, 0.4f));
        p.life = p.maxLife;
        p.startSize = defaultStartSize * (0.8f + glm::linearRand(0.0f, 0.4f));
        p.endSize = defaultEndSize;
        p.size = p.startSize;
        mParticles.push_back(p);
    }
}

void ParticleEmitter::EmitBurst(const Vec3& origin) {
    Emit(defaultCount, origin, defaultDirection, defaultSpread, defaultSpeed, defaultLife, defaultStartColor);
    // Fix end color for burst to use preset end color
    size_t start = mParticles.size() > static_cast<size_t>(defaultCount)
        ? mParticles.size() - static_cast<size_t>(defaultCount) : 0;
    for (size_t i = start; i < mParticles.size(); ++i) {
        mParticles[i].startColor = defaultStartColor;
        mParticles[i].endColor = defaultEndColor;
        mParticles[i].startSize = defaultStartSize;
        mParticles[i].endSize = defaultEndSize;
    }
}

void ParticleEmitter::Update(float dt) {
    for (auto& p : mParticles) {
        if (useGravity) {
            p.velocity.y -= gravity * dt;
        }
        if (drag > 0.0f) {
            p.velocity *= std::max(0.0f, 1.0f - drag * dt);
        }
        p.position += p.velocity * dt;
        p.life -= dt;
        float t = (p.maxLife > 1e-5f) ? (1.0f - p.life / p.maxLife) : 1.0f;
        t = glm::clamp(t, 0.0f, 1.0f);
        p.color = glm::mix(p.startColor, p.endColor, t);
        p.size = glm::mix(p.startSize, p.endSize, t);
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
