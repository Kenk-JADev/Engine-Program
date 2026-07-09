#pragma once

#include "Types.h"

namespace rpg {

class Camera {
public:
    Camera();

    void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);
    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

    void SetPosition(const Vec3& pos) { mPosition = pos; mDirty = true; }
    void SetRotation(const Vec3& rot) { mRotation = rot; mDirty = true; }
    void LookAt(const Vec3& target, const Vec3& up = Vec3(0, 1, 0));

    const Vec3& GetPosition() const { return mPosition; }
    const Vec3& GetRotation() const { return mRotation; }
    Vec3 GetForward() const;
    Vec3 GetRight() const;
    Vec3 GetUp() const;

    const Mat4& GetViewMatrix() const;
    const Mat4& GetProjectionMatrix() const { return mProjection; }

private:
    void Recalculate() const;

    Vec3 mPosition{0.0f, 5.0f, 10.0f};
    Vec3 mRotation{-25.0f, 0.0f, 0.0f};
    mutable Mat4 mView{1.0f};
    Mat4 mProjection{1.0f};
    mutable bool mDirty = true;
};

} // namespace rpg
