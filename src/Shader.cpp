#include "rpgmaker3d/Shader.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace rpg {

Shader::Shader() = default;

Shader::~Shader() {
    Delete();
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::ifstream vf(vertexPath), ff(fragmentPath);
    if (!vf.is_open() || !ff.is_open()) {
        std::cerr << "Failed to open shader files: " << vertexPath << ", " << fragmentPath << std::endl;
        return false;
    }
    std::stringstream vss, fss;
    vss << vf.rdbuf();
    fss << ff.rdbuf();
    return LoadFromSource(vss.str(), fss.str());
}

bool Shader::LoadFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
    Delete();
    mProgramID = glCreateProgram();

    GLuint vs = 0, fs = 0;
    if (!CompileStage(vs, GL_VERTEX_SHADER, vertexSource)) return false;
    if (!CompileStage(fs, GL_FRAGMENT_SHADER, fragmentSource)) return false;

    glAttachShader(mProgramID, vs);
    glAttachShader(mProgramID, fs);
    glLinkProgram(mProgramID);

    GLint success;
    glGetProgramiv(mProgramID, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(mProgramID, 512, nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return true;
}

bool Shader::CompileStage(GLuint& stage, GLuint type, const std::string& source) {
    stage = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(stage, 1, &src, nullptr);
    glCompileShader(stage);

    GLint success;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(stage, 512, nullptr, log);
        std::cerr << "Shader compile error (" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << "): " << log << std::endl;
        return false;
    }
    return true;
}

void Shader::Bind() const {
    glUseProgram(mProgramID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::Delete() {
    if (mProgramID != 0) {
        glDeleteProgram(mProgramID);
        mProgramID = 0;
    }
    mUniformCache.clear();
}

GLint Shader::GetUniformLocation(const std::string& name) {
    auto it = mUniformCache.find(name);
    if (it != mUniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(mProgramID, name.c_str());
    mUniformCache[name] = loc;
    return loc;
}

void Shader::SetInt(const std::string& name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const Vec2& value) {
    glUniform2f(GetUniformLocation(name), value.x, value.y);
}

void Shader::SetVec3(const std::string& name, const Vec3& value) {
    glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
}

void Shader::SetVec4(const std::string& name, const Vec4& value) {
    glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
}

void Shader::SetMat4(const std::string& name, const Mat4& value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetBool(const std::string& name, bool value) {
    glUniform1i(GetUniformLocation(name), value ? 1 : 0);
}

} // namespace rpg
