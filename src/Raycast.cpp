#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace rpg {

Ray Raycast::ScreenPointToRay(const Camera& camera, const Vec2& screenPos, const Vec2& screenSize) {
    float x = (2.0f * screenPos.x) / std::max(1.0f, screenSize.x) - 1.0f;
    float y = 1.0f - (2.0f * screenPos.y) / std::max(1.0f, screenSize.y);

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
    hit.normal = glm::normalize(planeNormal);
    return hit;
}

RaycastHit Raycast::IntersectBoundingBox(const Ray& ray, const Vec3& min, const Vec3& max) {
    // Avoid division by zero
    Vec3 invDir(
        (std::abs(ray.direction.x) < 1e-8f) ? 1e8f : 1.0f / ray.direction.x,
        (std::abs(ray.direction.y) < 1e-8f) ? 1e8f : 1.0f / ray.direction.y,
        (std::abs(ray.direction.z) < 1e-8f) ? 1e8f : 1.0f / ray.direction.z
    );

    float tmin = (min.x - ray.origin.x) * invDir.x;
    float tmax = (max.x - ray.origin.x) * invDir.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (min.y - ray.origin.y) * invDir.y;
    float tymax = (max.y - ray.origin.y) * invDir.y;
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return {};
    tmin = std::max(tmin, tymin);
    tmax = std::min(tmax, tymax);

    float tzmin = (min.z - ray.origin.z) * invDir.z;
    float tzmax = (max.z - ray.origin.z) * invDir.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return {};
    tmin = std::max(tmin, tzmin);

    if (tmax < 0.0f) return {};
    float t = (tmin >= 0.0f) ? tmin : tmax;
    if (t < 0.0f) return {};

    RaycastHit hit;
    hit.hit = true;
    hit.distance = t;
    hit.point = ray.origin + ray.direction * t;
    // Approximate normal from which face was hit
    Vec3 c = (min + max) * 0.5f;
    Vec3 d = hit.point - c;
    Vec3 ext = (max - min) * 0.5f;
    float ax = std::abs(d.x / std::max(1e-5f, ext.x));
    float ay = std::abs(d.y / std::max(1e-5f, ext.y));
    float az = std::abs(d.z / std::max(1e-5f, ext.z));
    if (ax > ay && ax > az) hit.normal = Vec3((d.x > 0) ? 1.0f : -1.0f, 0, 0);
    else if (ay > az) hit.normal = Vec3(0, (d.y > 0) ? 1.0f : -1.0f, 0);
    else hit.normal = Vec3(0, 0, (d.z > 0) ? 1.0f : -1.0f);
    return hit;
}

RaycastHit Raycast::IntersectSphere(const Ray& ray, const Vec3& center, float radius) {
    Vec3 oc = ray.origin - center;
    float b = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0f) return {};
    float t = -b - std::sqrt(disc);
    if (t < 0.0f) t = -b + std::sqrt(disc);
    if (t < 0.0f) return {};
    RaycastHit hit;
    hit.hit = true;
    hit.distance = t;
    hit.point = ray.PointAt(t);
    hit.normal = glm::normalize(hit.point - center);
    return hit;
}

RaycastHit Raycast::PickEntity(const Ray& ray, Scene& scene, float maxDistance) {
    RaycastHit best;
    best.distance = maxDistance;
    for (EntityID id : scene.GetEntities()) {
        auto* transform = scene.GetComponent<TransformComponent>(id);
        if (!transform) continue;
        Vec3 pos = transform->transform.position;
        Vec3 sc = transform->transform.scale;
        Vec3 half = Vec3(0.5f) * sc;
        // Expand slightly for easier picking
        half = glm::max(half, Vec3(0.25f));
        RaycastHit h = IntersectBoundingBox(ray, pos - half, pos + half);
        if (h.hit && h.distance < best.distance) {
            best = h;
            best.entity = id;
        }
    }
    return best;
}

float Raycast::DistanceToSegment(const Ray& ray, const Vec3& a, const Vec3& b, float* outT) {
    // Closest distance between ray and segment AB
    Vec3 u = ray.direction;
    Vec3 v = b - a;
    Vec3 w = ray.origin - a;
    float aa = glm::dot(u, u);
    float ad = glm::dot(u, v);
    float bb = glm::dot(v, v);
    float ae = glm::dot(u, w);
    float be = glm::dot(v, w);
    float denom = aa * bb - ad * ad;
    float sc, tc;
    if (denom < 1e-8f) {
        sc = 0.0f;
        tc = (bb > 1e-8f) ? be / bb : 0.0f;
    } else {
        sc = (ad * be - bb * ae) / denom;
        tc = (aa * be - ad * ae) / denom;
    }
    sc = std::max(0.0f, sc);
    tc = glm::clamp(tc, 0.0f, 1.0f);
    if (outT) *outT = tc;
    Vec3 dP = w + u * sc - v * tc;
    return glm::length(dP);
}

} // namespace rpg
