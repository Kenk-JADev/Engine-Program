#pragma once

#include "Types.h"

namespace rpg {

class Camera;

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

struct RaycastHit {
    bool hit = false;
    Vec3 point;
    float distance = 0.0f;
};

class Raycast {
public:
    static Ray ScreenPointToRay(const Camera& camera, const Vec2& screenPos, const Vec2& screenSize);
    static RaycastHit IntersectPlane(const Ray& ray, const Vec3& planeNormal, const Vec3& planePoint);
    static RaycastHit IntersectBoundingBox(const Ray& ray, const Vec3& min, const Vec3& max);
};

} // namespace rpg
