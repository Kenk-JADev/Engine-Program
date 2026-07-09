#pragma once

#include <string>
#include <unordered_map>
#include "Types.h"
#include <glad/gl.h>

namespace rpg {

class Shader {
public:
    Shader();
    ~Shader();

    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool LoadFromSource(const std::string& vertexSource, const std::string& fragmentSource);
    void Bind() const;
    void Unbind() const;
    void Delete();

    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const Vec2& value);
    void SetVec3(const std::string& name, const Vec3& value);
    void SetVec4(const std::string& name, const Vec4& value);
    void SetMat4(const std::string& name, const Mat4& value);

    GLuint GetID() const { return mProgramID; }

private:
    bool CompileStage(GLuint& stage, GLuint type, const std::string& source);
    GLint GetUniformLocation(const std::string& name);

    GLuint mProgramID = 0;
    std::unordered_map<std::string, GLint> mUniformCache;
};

} // namespace rpg
