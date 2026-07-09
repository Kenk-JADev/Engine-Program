#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace rpg {

Ray Raycast::ScreenPointToRay(const Camera& camera, const Vec2& screenPos, const Vec2& screenSize) {
    float x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * screenPos.y) / screenSize.y;

    Vec4 rayClip(x, y, -1.0f, 1.0f);
    Vec4 rayEye = glm::inverse(camera.GetProjectionMatrix()) * rayClip;
    rayEye = Vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    Vec4 rayWorld = glm::inverse(camera.GetViewMatrix()) * rayEye;
    Vec3 rayDir = glm::normalize(Vec3(rayWorld));

    return { camera.GetPosition(), rayDir };
}

RaycastHit Raycast::IntersectPlane(const Ray& ray, const Vec3& planeNormal, const Vec3& planePoint) {
    float denom = glm::dot(ray.direction, planeNormal);
    if (glm::abs(denom) < 1e-6f) return {};

    float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
    if (t < 0.0f) return {};

    RaycastHit hit;
    hit.hit = true;
    hit.distance = t;
    hit.point = ray.origin + ray.direction * t;
    return hit;
}

RaycastHit Raycast::IntersectBoundingBox(const Ray& ray, const Vec3& min, const Vec3& max) {
    float tmin = (min.x - ray.origin.x) / ray.direction.x;
    float tmax = (max.x - ray.origin.x) / ray.direction.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (min.y - ray.origin.y) / ray.direction.y;
    float tymax = (max.y - ray.origin.y) / ray.direction.y;
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return {};
    tmin = std::max(tmin, tymin);
    tmax = std::min(tmax, tymax);

    float tzmin = (min.z - ray.origin.z) / ray.direction.z;
    float tzmax = (max.z - ray.origin.z) / ray.direction.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return {};
    tmin = std::max(tmin, tzmin);

    RaycastHit hit;
    hit.hit = true;
    hit.distance = tmin;
    hit.point = ray.origin + ray.direction * tmin;
    return hit;
}

} // namespace rpg
