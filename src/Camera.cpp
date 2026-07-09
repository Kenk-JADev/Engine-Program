#include "rpgmaker3d/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace rpg {

Camera::Camera() {
    SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
}

void Camera::SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    mProjection = glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
}

void Camera::SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    mProjection = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
}

void Camera::LookAt(const Vec3& target, const Vec3& up) {
    mView = glm::lookAt(mPosition, target, up);
    mDirty = false;
}

Vec3 Camera::GetForward() const {
    return Vec3(
        -sin(glm::radians(mRotation.y)) * cos(glm::radians(mRotation.x)),
        sin(glm::radians(mRotation.x)),
        -cos(glm::radians(mRotation.y)) * cos(glm::radians(mRotation.x))
    );
}

Vec3 Camera::GetRight() const {
    return glm::normalize(glm::cross(GetForward(), Vec3(0, 1, 0)));
}

Vec3 Camera::GetUp() const {
    return glm::normalize(glm::cross(GetRight(), GetForward()));
}

const Mat4& Camera::GetViewMatrix() const {
    if (mDirty) Recalculate();
    return mView;
}

void Camera::Recalculate() const {
    Vec3 forward = GetForward();
    Vec3 target = mPosition + forward;
    mView = glm::lookAt(mPosition, target, Vec3(0, 1, 0));
    mDirty = false;
}

} // namespace rpg
