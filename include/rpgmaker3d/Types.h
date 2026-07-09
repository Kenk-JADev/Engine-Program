#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace rpg {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;
using Color = glm::vec4;

struct Transform {
    Vec3 position{0.0f};
    Vec3 rotation{0.0f};
    Vec3 scale{1.0f};

    Mat4 GetMatrix() const {
        Mat4 m = glm::translate(Mat4(1.0f), position);
        m = glm::rotate(m, rotation.x, Vec3(1, 0, 0));
        m = glm::rotate(m, rotation.y, Vec3(0, 1, 0));
        m = glm::rotate(m, rotation.z, Vec3(0, 0, 1));
        m = glm::scale(m, scale);
        return m;
    }
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct Rect {
    int x, y, width, height;
};

using EntityID = uint32_t;
constexpr EntityID INVALID_ENTITY = 0;

} // namespace rpg
