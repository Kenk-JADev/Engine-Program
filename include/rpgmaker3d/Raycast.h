#pragma once

#include "Types.h"
#include <vector>
#include <limits>

namespace rpg {

class Camera;
class Scene;

struct Ray {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    Vec3 PointAt(float t) const { return origin + direction * t; }
};

struct RaycastHit {
    bool hit = false;
    Vec3 point{0.0f};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    EntityID entity = INVALID_ENTITY;
};

class Raycast {
public:
    static Ray ScreenPointToRay(const Camera& camera, const Vec2& screenPos, const Vec2& screenSize);
    static RaycastHit IntersectPlane(const Ray& ray, const Vec3& planeNormal, const Vec3& planePoint);
    static RaycastHit IntersectBoundingBox(const Ray& ray, const Vec3& min, const Vec3& max);
    static RaycastHit IntersectSphere(const Ray& ray, const Vec3& center, float radius);

    /// Closest entity hit by ray against unit AABB centered on transform (scale applied).
    static RaycastHit PickEntity(const Ray& ray, Scene& scene, float maxDistance = 1000.0f);

    /// Distance from ray to a world-space line segment (for gizmo axis picking).
    static float DistanceToSegment(const Ray& ray, const Vec3& a, const Vec3& b, float* outT = nullptr);
};

} // namespace rpg
