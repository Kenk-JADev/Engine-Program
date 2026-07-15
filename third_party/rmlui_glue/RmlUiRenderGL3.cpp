#include "RmlUiRenderGL3.h"

#include <glad/gl.h>
#include <stb_image.h>
#include <cstddef>
#include <cstring>

namespace rpg {

// ---------------------------------------------------------------------------
// Shaders (GLSL 330). UI positions are in pixels, top-left origin; the
// projection matrix maps them into clip space.
// ---------------------------------------------------------------------------
static const char* kUiVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uProj;
uniform vec2 uTranslate;
uniform mat4 uTransform;
uniform int uUseTransform;

out vec4 vColor;
out vec2 vTexCoord;

void main() {
    vec2 p = aPosition + uTranslate;
    vec4 pos = (uUseTransform != 0) ? (uTransform * vec4(p, 0.0, 1.0)) : vec4(p, 0.0, 1.0);
    gl_Position = uProj * pos;
    vColor = aColor;
    vTexCoord = aTexCoord;
}
)";

static const char* kUiFragmentShader = R"(
#version 330 core
in vec4 vColor;
in vec2 vTexCoord;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    // RmlUi uses premultiplied alpha throughout (blend: ONE, ONE_MINUS_SRC_ALPHA).
    FragColor = vColor * texture(uTexture, vTexCoord);
}
)";

RmlUiRenderGL3::RmlUiRenderGL3() = default;

RmlUiRenderGL3::~RmlUiRenderGL3() {
    Shutdown();
}

bool RmlUiRenderGL3::Initialize() {
    mProgram = CompileShaderProgram();
    if (!mProgram) return false;

    mLocProj = glGetUniformLocation(mProgram, "uProj");
    mLocTranslate = glGetUniformLocation(mProgram, "uTranslate");
    mLocTransform = glGetUniformLocation(mProgram, "uTransform");
    mLocUseTransform = glGetUniformLocation(mProgram, "uUseTransform");
    mLocTexture = glGetUniformLocation(mProgram, "uTexture");

    mWhiteTexture = CreateWhiteTexture();
    SetTransform(nullptr);
    return mWhiteTexture != 0;
}

void RmlUiRenderGL3::Shutdown() {
    if (mWhiteTexture) { glDeleteTextures(1, &mWhiteTexture); mWhiteTexture = 0; }
    if (mProgram) { glDeleteProgram(mProgram); mProgram = 0; }
}

void RmlUiRenderGL3::SetViewport(int width, int height) {
    mWidth = width;
    mHeight = height;
}

void RmlUiRenderGL3::BeginRender() {
    // --- save engine state ---
    glGetIntegerv(GL_CURRENT_PROGRAM, &mSaved.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mSaved.vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &mSaved.arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &mSaved.elementBuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &mSaved.texture2D);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &mSaved.activeTexture);
    glGetIntegerv(GL_BLEND, &mSaved.blend);
    glGetIntegerv(GL_DEPTH_TEST, &mSaved.depthTest);
    glGetIntegerv(GL_SCISSOR_TEST, &mSaved.scissorTest);
    glGetIntegerv(GL_CULL_FACE, &mSaved.cullFace);
    glGetIntegerv(GL_VIEWPORT, mSaved.viewport);
    glGetIntegerv(GL_SCISSOR_BOX, mSaved.scissorBox);
    glGetIntegerv(GL_BLEND_SRC_RGB, &mSaved.blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &mSaved.blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSaved.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &mSaved.blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &mSaved.blendEqRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &mSaved.blendEqAlpha);

    // --- configure for UI rendering ---
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied alpha (like official backend)
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glViewport(mSaved.viewport[0], mSaved.viewport[1], mSaved.viewport[2], mSaved.viewport[3]);

    // Pixel ortho projection: x: 0..w -> -1..1, y: 0..h -> 1..-1 (top-left origin)
    const float l = 0.0f, r = (float)mWidth, t = 0.0f, b = (float)mHeight;
    float proj[16] = {0};
    proj[0] = 2.0f / (r - l);
    proj[5] = 2.0f / (t - b);
    proj[10] = -1.0f;
    proj[12] = -(r + l) / (r - l);
    proj[13] = -(t + b) / (t - b);
    proj[15] = 1.0f;

    glUseProgram(mProgram);
    glUniformMatrix4fv(mLocProj, 1, GL_FALSE, proj);
    glUniform1i(mLocTexture, 0);
    glActiveTexture(GL_TEXTURE0);
}

void RmlUiRenderGL3::EndRender() {
    // --- restore engine state ---
    glUseProgram((GLuint)mSaved.program);
    glBindVertexArray((GLuint)mSaved.vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mSaved.arrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)mSaved.elementBuffer);
    glActiveTexture((GLenum)mSaved.activeTexture);
    glBindTexture(GL_TEXTURE_2D, (GLuint)mSaved.texture2D);

    glBlendEquationSeparate((GLenum)mSaved.blendEqRGB, (GLenum)mSaved.blendEqAlpha);
    glBlendFuncSeparate((GLenum)mSaved.blendSrcRGB, (GLenum)mSaved.blendDstRGB,
                        (GLenum)mSaved.blendSrcAlpha, (GLenum)mSaved.blendDstAlpha);
    if (mSaved.blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (mSaved.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (mSaved.cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (mSaved.scissorTest) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glScissor(mSaved.scissorBox[0], mSaved.scissorBox[1], mSaved.scissorBox[2], mSaved.scissorBox[3]);
    glViewport(mSaved.viewport[0], mSaved.viewport[1], mSaved.viewport[2], mSaved.viewport[3]);
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
Rml::CompiledGeometryHandle RmlUiRenderGL3::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                            Rml::Span<const int> indices) {
    auto* g = new Geometry();
    glGenVertexArrays(1, &g->vao);
    glGenBuffers(1, &g->vbo);
    glGenBuffers(1, &g->ibo);

    glBindVertexArray(g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(Rml::Vertex)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(int)), indices.data(), GL_STATIC_DRAW);

    // Rml::Vertex layout: pos vec2f @0 | colour byte4 @8 | uv vec2f @12 ; stride 20
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Rml::Vertex), (const void*)offsetof(Rml::Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)sizeof(Rml::Vertex), (const void*)offsetof(Rml::Vertex, colour));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Rml::Vertex), (const void*)offsetof(Rml::Vertex, tex_coord));

    glBindVertexArray(0);
    g->numIndices = (int)indices.size();
    return (Rml::CompiledGeometryHandle)g;
}

void RmlUiRenderGL3::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                                    Rml::TextureHandle texture) {
    auto* g = (Geometry*)geometry;
    if (!g) return;

    glUseProgram(mProgram);
    glUniform2f(mLocTranslate, translation.x, translation.y);
    glUniformMatrix4fv(mLocTransform, 1, GL_FALSE, mTransform.data());
    glUniform1i(mLocUseTransform, mTransformActive ? 1 : 0);

    glBindTexture(GL_TEXTURE_2D, texture ? (GLuint)texture : mWhiteTexture);

    if (mScissorEnabled) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(mScissor.Left(), mScissor.Top(), mScissor.Width(), mScissor.Height());
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    glBindVertexArray(g->vao);
    glDrawElements(GL_TRIANGLES, g->numIndices, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void RmlUiRenderGL3::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    auto* g = (Geometry*)geometry;
    if (!g) return;
    glDeleteVertexArrays(1, &g->vao);
    glDeleteBuffers(1, &g->vbo);
    glDeleteBuffers(1, &g->ibo);
    delete g;
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
Rml::TextureHandle RmlUiRenderGL3::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0); // RmlUi expects top-left origin data
    unsigned char* data = stbi_load(source.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!data) return (Rml::TextureHandle)0;
    texture_dimensions = Rml::Vector2i(w, h);
    GLuint tex = CreateTextureFromRGBA(data, w, h);
    stbi_image_free(data);
    return (Rml::TextureHandle)tex;
}

Rml::TextureHandle RmlUiRenderGL3::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
    if (source_dimensions.x <= 0 || source_dimensions.y <= 0 || source.empty()) return (Rml::TextureHandle)0;
    GLuint tex = CreateTextureFromRGBA(source.data(), source_dimensions.x, source_dimensions.y);
    return (Rml::TextureHandle)tex;
}

void RmlUiRenderGL3::ReleaseTexture(Rml::TextureHandle texture) {
    GLuint t = (GLuint)texture;
    if (t) glDeleteTextures(1, &t);
}

// ---------------------------------------------------------------------------
// Scissor / transform
// ---------------------------------------------------------------------------
void RmlUiRenderGL3::EnableScissorRegion(bool enable) {
    mScissorEnabled = enable;
}

void RmlUiRenderGL3::SetScissorRegion(Rml::Rectanglei region) {
    // Convert RmlUi top-left origin rect to GL bottom-left origin.
    mScissor = Rml::Rectanglei::FromPositionSize(
        Rml::Vector2i(region.Left(), mHeight - region.Top() - region.Height()),
        Rml::Vector2i(region.Width(), region.Height()));
}

void RmlUiRenderGL3::SetTransform(const Rml::Matrix4f* transform) {
    if (transform) {
        mTransform = *transform;
        mTransformActive = true;
    } else {
        mTransform = Rml::Matrix4f::Identity();
        mTransformActive = false;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
unsigned int RmlUiRenderGL3::CompileShaderProgram() {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { glDeleteShader(s); return 0; }
        return s;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, kUiVertexShader);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kUiFragmentShader);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(prog); return 0; }
    return prog;
}

unsigned int RmlUiRenderGL3::CreateWhiteTexture() {
    const unsigned char white[4] = {255, 255, 255, 255};
    return CreateTextureFromRGBA(white, 1, 1);
}

unsigned int RmlUiRenderGL3::CreateTextureFromRGBA(const unsigned char* data, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

} // namespace rpg
