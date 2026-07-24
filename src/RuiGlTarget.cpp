// RPG Maker 3D - RUI-GL-DrawTarget (PAKET 39) - siehe include/rpgmaker3d/RuiGlTarget.h
#include "rpgmaker3d/RuiGlTarget.h"
#include "rpgmaker3d/Font8x8.h"     // kFont8x8Basic (Public Domain)
#include "rpgmaker3d/Logger.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rpg {

namespace {

// --- Shader (GLSL 330 core; px -> NDC per Uniform uDisplay) ----------------
const char* kRuiVs = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aCol;
uniform vec2 uDisplay;
out vec2 vUv;
out vec4 vCol;
void main() {
    vUv = aUv;
    vCol = aCol;
    vec2 ndc = vec2(aPos.x / uDisplay.x * 2.0 - 1.0,
                    1.0 - aPos.y / uDisplay.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

const char* kRuiFs = R"(#version 330 core
in vec2 vUv;
in vec4 vCol;
uniform sampler2D uTex;
out vec4 oCol;
void main() {
    oCol = texture(uTex, vUv) * vCol;
}
)";

unsigned int CompileShaderStage(GLenum type, const char* src, const char* what) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512]; GLsizei len = 0;
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), &len, log);
        RPG_LOG_ERROR(std::string("RuiGlTarget: Shader-Fehler (") + what + "): " +
                      std::string(log, (size_t)len));
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

} // namespace

// --- Glyph-Dekodierung ------------------------------------------------------
// UTF-8-Codepoint -> Atlas-Index (0..255). ASCII direkt; die deutschen
// Umlaute und sz bekommen synthetisierte Plaetze 128..134 (Diaerese wird
// beim Atlas-Bau aus dem Basis-Buchstaben gezogen); Rest -> '?'.
int RuiGlTarget::DecodeGlyph(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { ++i; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        const unsigned int cp = ((c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
        i += 2;
        switch (cp) {
            case 0xE4: return 128; // ae
            case 0xF6: return 129; // oe
            case 0xFC: return 130; // ue
            case 0xC4: return 131; // Ae
            case 0xD6: return 132; // Oe
            case 0xDC: return 133; // Ue
            case 0xDF: return 134; // sz
            default:   return '?';
        }
    }
    size_t adv = 1;
    if ((c & 0xF0) == 0xE0) adv = 3;
    else if ((c & 0xF8) == 0xF0) adv = 4;
    i += (i + adv <= s.size()) ? adv : 1;
    return '?';
}

void RuiGlTarget::GlyphUv(int glyph, float& u0, float& v0, float& u1, float& v1) const {
    const int g = (glyph >= 0 && glyph < 256) ? glyph : (int)'?';
    const float col = (float)(g % 16), row = (float)(g / 16);
    const float cell = 8.0f / 128.0f;
    u0 = col * cell;       v0 = row * cell;
    u1 = (col + 1) * cell; v1 = (row + 1) * cell;
}

bool RuiGlTarget::BuildFontAtlas() {
    // 256 Glyphen a 8x8 in einem 128x128-RGBA-Raster; weiss in RGB,
    // Glyph-Bit in Alpha (fragmentseitig: Farbe * Alpha).
    uint8_t px[128 * 128 * 4];
    std::memset(px, 255, sizeof(px));
    // Erst alles transparent, dann gesetzte Bits opak:
    for (size_t i = 3; i < sizeof(px); i += 4) px[i] = 0;

    uint8_t bits[256][8];
    std::memset(bits, 0, sizeof(bits));
    for (int g = 0; g < 128; ++g)
        for (int row = 0; row < 8; ++row)
            bits[g][row] = kFont8x8Basic[g][row];
    // Umlaute: Basis-Glyph + Diaerese (Zeile 0, x=2 und x=5); Gross-
    // buchstaben werden eine Zeile nach unten geschoben, damit die Punkte
    // nicht kollidieren.
    auto dieresis = [&](int dst, int src, bool shiftDown) {
        for (int row = 0; row < 8; ++row) {
            if (row == 0) {
                bits[dst][row] = (uint8_t)((1u << (7 - 2)) | (1u << (7 - 5)));
            } else {
                int sr = shiftDown ? (row - 1) : row;
                if (sr < 0) sr = 0;
                bits[dst][row] = kFont8x8Basic[src][sr];
            }
        }
    };
    dieresis(128, 'a', false);
    dieresis(129, 'o', false);
    dieresis(130, 'u', false);
    dieresis(131, 'A', true);
    dieresis(132, 'O', true);
    dieresis(133, 'U', true);
    for (int row = 0; row < 8; ++row) bits[134][row] = kFont8x8Basic['s'][row]; // sz-Naeherung
    for (int g = 135; g < 256; ++g)
        for (int row = 0; row < 8; ++row)
            bits[g][row] = kFont8x8Basic['?'][row];

    for (int g = 0; g < 256; ++g) {
        const int cx = (g % 16) * 8, cy = (g / 16) * 8;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if ((bits[g][row] >> (7 - col)) & 1) {
                    const size_t idx = ((size_t)(cy + row) * 128u + (size_t)(cx + col)) * 4u;
                    px[idx + 3] = 255; // nur Alpha setzen (RGB bleibt weiss)
                }
            }
        }
    }

    glGenTextures(1, &mFontTex);
    glBindTexture(GL_TEXTURE_2D, mFontTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return mFontTex != 0;
}

// --- Init/Shutdown ----------------------------------------------------------
bool RuiGlTarget::Init() {
    if (mReady) return true;
    if (mTried) return false;
    mTried = true;

    const GLuint vs = CompileShaderStage(GL_VERTEX_SHADER, kRuiVs, "vs");
    const GLuint fs = CompileShaderStage(GL_FRAGMENT_SHADER, kRuiFs, "fs");
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return false; }
    mShader = glCreateProgram();
    glAttachShader(mShader, vs);
    glAttachShader(mShader, fs);
    glLinkProgram(mShader);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(mShader, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        RPG_LOG_ERROR("RuiGlTarget: Shader-Link fehlgeschlagen");
        glDeleteProgram(mShader); mShader = 0;
        return false;
    }
    mULocDisplay = glGetUniformLocation(mShader, "uDisplay");
    mULocTex = glGetUniformLocation(mShader, "uTex");

    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    const GLsizei stride = (GLsizei)sizeof(Vert);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vert, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vert, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vert, r));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 1x1-Weisstextur fuer unbelegte Primitive
    const unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &mWhiteTex);
    glBindTexture(GL_TEXTURE_2D, mWhiteTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);

    if (!BuildFontAtlas()) return false;

    mVerts.reserve(16384);
    mReady = true;
    RPG_LOG_INFO("RuiGlTarget: eigener GL-Renderer bereit (ImGui nicht noetig)");
    return true;
}

void RuiGlTarget::Shutdown() {
    if (mFontTex) { glDeleteTextures(1, &mFontTex); mFontTex = 0; }
    if (mWhiteTex) { glDeleteTextures(1, &mWhiteTex); mWhiteTex = 0; }
    if (mVbo) { glDeleteBuffers(1, &mVbo); mVbo = 0; }
    if (mVao) { glDeleteVertexArrays(1, &mVao); mVao = 0; }
    if (mShader) { glDeleteProgram(mShader); mShader = 0; }
    mReady = false;
}

RuiGlTarget::~RuiGlTarget() { Shutdown(); }

// --- Frameklammer ------------------------------------------------------------
void RuiGlTarget::BeginFrame(float displayW, float displayH) {
    if (!mReady || mFrameOpen) return;
    mFrameOpen = true;
    mDispW = displayW > 0.0f ? displayW : 1280.0f;
    mDispH = displayH > 0.0f ? displayH : 720.0f;
    mVerts.clear();
    mClipStack.clear();
    mCurTex = 0xFFFFFFFFu;

    glGetIntegerv(GL_CURRENT_PROGRAM, &mSaved.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mSaved.vao);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &mSaved.tex2d);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &mSaved.activeTex);
    mSaved.blend = glIsEnabled(GL_BLEND) == GL_TRUE;
    mSaved.depth = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    mSaved.cull = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    mSaved.scissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    glGetIntegerv(GL_BLEND_SRC_RGB, &mSaved.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &mSaved.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSaved.blendSrcA);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &mSaved.blendDstA);
    // PAKET 41: Scissor-Box + Array-Buffer sichern (Hygiene, PAKET 39)
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &mSaved.arrayBuf);
    glGetIntegerv(GL_SCISSOR_BOX, mSaved.scissorBox);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
}

void RuiGlTarget::EndFrame() {
    if (!mReady || !mFrameOpen) return;
    Flush();
    glDisable(GL_SCISSOR_TEST);

    glUseProgram((GLuint)mSaved.program);
    glBindVertexArray((GLuint)mSaved.vao);
    glActiveTexture((GLenum)mSaved.activeTex);
    glBindTexture(GL_TEXTURE_2D, (GLuint)mSaved.tex2d);
    if (mSaved.blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (mSaved.depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (mSaved.cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (mSaved.scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    // PAKET 41: Scissor-Box + Array-Buffer-Bindung restaurieren — sonst
    // erbte der Host (z. B. ein spaeterer Pass mit eigenem Scissor)
    // unsere letzte Clip-Box bzw. ein gebundenes Staging-VBO.
    if (mSaved.scissor)
        glScissor(mSaved.scissorBox[0], mSaved.scissorBox[1],
                  (GLsizei)mSaved.scissorBox[2], (GLsizei)mSaved.scissorBox[3]);
    glBlendFuncSeparate((GLenum)mSaved.blendSrcRgb, (GLenum)mSaved.blendDstRgb,
                        (GLenum)mSaved.blendSrcA, (GLenum)mSaved.blendDstA);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mSaved.arrayBuf);
    mFrameOpen = false;
}

// --- Batch -------------------------------------------------------------------
void RuiGlTarget::PushVert(float x, float y, float u, float v, const rui::Color4& c) {
    mVerts.push_back(Vert{x, y, u, v, c.r, c.g, c.b, c.a});
    if (mVerts.size() >= 32768) Flush(); // Sicherheitsnetz bei langen Frames
}

void RuiGlTarget::PushTri(float x0, float y0, float x1, float y1,
                          float x2, float y2, const rui::Color4& c) {
    UseTexture(mWhiteTex);
    PushVert(x0, y0, 0.0f, 0.0f, c);
    PushVert(x1, y1, 0.0f, 0.0f, c);
    PushVert(x2, y2, 0.0f, 0.0f, c);
}

void RuiGlTarget::PushQuadUV(const float px[4], const float py[4],
                             float u0, float v0, float u1, float v1,
                             const rui::Color4& c) {
    // Ecken 0..3: oben-links, oben-rechts, unten-rechts, unten-links
    PushVert(px[0], py[0], u0, v0, c);
    PushVert(px[1], py[1], u1, v0, c);
    PushVert(px[2], py[2], u1, v1, c);
    PushVert(px[0], py[0], u0, v0, c);
    PushVert(px[2], py[2], u1, v1, c);
    PushVert(px[3], py[3], u0, v1, c);
}

void RuiGlTarget::UseTexture(unsigned int tex) {
    if (tex == mCurTex) return;
    Flush();
    mCurTex = tex;
}

void RuiGlTarget::Flush() {
    if (!mReady || mVerts.empty()) return;
    glUseProgram(mShader);
    glUniform2f(mULocDisplay, mDispW, mDispH);
    glUniform1i(mULocTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mCurTex == 0xFFFFFFFFu ? mWhiteTex : mCurTex);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mVerts.size() * sizeof(Vert)),
                 mVerts.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mVerts.size());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    mVerts.clear();
}

// --- Primitive ----------------------------------------------------------------
void RuiGlTarget::FillRect(const rui::Rect& r, const rui::Color4& c, float rounding) {
    if (rounding < 0.5f || r.w < 2.0f * rounding || r.h < 2.0f * rounding) {
        const float px[4] = {r.x, r.x + r.w, r.x + r.w, r.x};
        const float py[4] = {r.y, r.y, r.y + r.h, r.y + r.h};
        UseTexture(mWhiteTex);
        PushQuadUV(px, py, 0, 0, 0, 0, c);
        return;
    }
    // Abgerundetes Rechteck: Flaeche als Fan-Polygon um die Mitte. Ecken im
    // Umlauf (Bildschirm-y nach unten): ro -> lo -> lu -> ru, je startA..+90.
    // Winkel->Punkt: (cx + cos*rad, cy - sin*rad):
    //   ro: 0=rechter Rand .. 90=oben | lo: 90=oben .. 180=links
    //   lu: 180=links .. 270=unten    | ru: 270=unten .. 360=rechts
    const int seg = 4;
    const float rad = rounding;
    const float cx[4] = {r.x + r.w - rad, r.x + rad,
                         r.x + rad,         r.x + r.w - rad};
    const float cy[4] = {r.y + rad,         r.y + rad,
                         r.y + r.h - rad,   r.y + r.h - rad};
    const float a0[4] = {0.0f, 90.0f, 180.0f, 270.0f};
    UseTexture(mWhiteTex);
    const float mx = r.x + r.w * 0.5f, my = r.y + r.h * 0.5f;
    float prevX = r.x + r.w, prevY = r.y + rad; // erster Bogenpunkt (ro, a=0)
    for (int corner = 0; corner < 4; ++corner) {
        for (int k = 0; k <= seg; ++k) {
            const float a = (a0[corner] + (float)k * (90.0f / (float)seg)) *
                            3.14159265f / 180.0f;
            const float vx = cx[corner] + std::cos(a) * rad;
            const float vy = cy[corner] - std::sin(a) * rad;
            if (corner > 0 || k > 0) PushTri(mx, my, prevX, prevY, vx, vy, c);
            prevX = vx; prevY = vy;
        }
    }
    // Faecher schliessen: letzter Punkt (ru, a=360 -> rechter Rand unten)
    // zurueck zum ersten (rechter Rand oben).
    PushTri(mx, my, prevX, prevY, r.x + r.w, r.y + rad, c);
}

void RuiGlTarget::StrokeRect(const rui::Rect& r, const rui::Color4& c, float t, float rounding) {
    (void)rounding; // Linien-Rahmen: Rundung wird bewusst nicht angedeutet
    Line(r.x, r.y, r.x + r.w, r.y, c, t);
    Line(r.x + r.w, r.y, r.x + r.w, r.y + r.h, c, t);
    Line(r.x + r.w, r.y + r.h, r.x, r.y + r.h, c, t);
    Line(r.x, r.y + r.h, r.x, r.y, c, t);
}

void RuiGlTarget::Line(float x0, float y0, float x1, float y1,
                       const rui::Color4& c, float thickness) {
    float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    const float half = std::max(0.5f, thickness * 0.5f);
    const float nx = -dy / len * half, ny = dx / len * half;
    const float px[4] = {x0 + nx, x1 + nx, x1 - nx, x0 - nx};
    const float py[4] = {y0 + ny, y1 + ny, y1 - ny, y0 - ny};
    UseTexture(mWhiteTex);
    PushQuadUV(px, py, 0, 0, 0, 0, c);
}

void RuiGlTarget::FillCircle(float cx, float cy, float radius, const rui::Color4& c) {
    const int seg = 18;
    for (int k = 0; k < seg; ++k) {
        const float a0 = (float)k / (float)seg * 2.0f * 3.14159265f;
        const float a1 = (float)(k + 1) / (float)seg * 2.0f * 3.14159265f;
        PushTri(cx, cy,
                cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
                cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, c);
    }
}

void RuiGlTarget::Text(float x, float y, const std::string& s,
                       const rui::Color4& c, float scale, int align) {
    const float w = MeasureText(s, scale);
    if (align == 1) x -= w * 0.5f;
    else if (align == 2) x -= w;
    UseTexture(mFontTex);
    const float cell = 8.0f * scale;
    size_t i = 0;
    float pen = x;
    while (i < s.size()) {
        const int glyph = DecodeGlyph(s, i);
        float u0, v0, u1, v1;
        GlyphUv(glyph, u0, v0, u1, v1);
        const float px[4] = {pen, pen + cell, pen + cell, pen};
        const float py[4] = {y, y, y + cell, y + cell};
        PushQuadUV(px, py, u0, v0, u1, v1, c);
        pen += cell;
    }
}

float RuiGlTarget::MeasureText(const std::string& s, float scale) const {
    size_t i = 0;
    int count = 0;
    while (i < s.size()) { DecodeGlyph(s, i); ++count; }
    return (float)count * 8.0f * scale;
}

float RuiGlTarget::LineHeight(float scale) const {
    return 8.0f * scale;
}

void RuiGlTarget::Image(void* texture, int imgW, int imgH,
                        const rui::Rect& src, const rui::Rect& dst,
                        const rui::Color4& tint, float rotationDeg) {
    if (!texture) return;
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    if (imgW > 0 && imgH > 0) {
        u0 = src.x / (float)imgW;           v0 = src.y / (float)imgH;
        u1 = (src.x + src.w) / (float)imgW; v1 = (src.y + src.h) / (float)imgH;
    }
    float px[4], py[4];
    if (std::fabs(rotationDeg) < 0.001f) {
        px[0] = dst.x;          py[0] = dst.y;
        px[1] = dst.x + dst.w;  py[1] = dst.y;
        px[2] = dst.x + dst.w;  py[2] = dst.y + dst.h;
        px[3] = dst.x;          py[3] = dst.y + dst.h;
    } else {
        const float rad = rotationDeg * 3.14159265f / 180.0f;
        const float cs = std::cos(rad), sn = std::sin(rad);
        const float mx = dst.x + dst.w * 0.5f, my = dst.y + dst.h * 0.5f;
        const float hx = dst.w * 0.5f, hy = dst.h * 0.5f;
        const float lx[4] = {-hx, hx, hx, -hx};
        const float ly[4] = {-hy, -hy, hy, hy};
        for (int k = 0; k < 4; ++k) {
            px[k] = mx + lx[k] * cs - ly[k] * sn;
            py[k] = my + lx[k] * sn + ly[k] * cs;
        }
    }
    UseTexture((unsigned int)(uintptr_t)texture);
    PushQuadUV(px, py, u0, v0, u1, v1, tint);
}

// --- Clipping ------------------------------------------------------------------
void RuiGlTarget::ApplyClip() {
    if (mClipStack.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    rui::Rect r = mClipStack.back();
    for (size_t k = 0; k + 1 < mClipStack.size(); ++k) {
        // mit allen darunter schneiden
        const rui::Rect& o = mClipStack[k];
        const float x0 = std::max(r.x, o.x), y0 = std::max(r.y, o.y);
        const float x1 = std::min(r.x + r.w, o.x + o.w);
        const float y1 = std::min(r.y + r.h, o.y + o.h);
        r = rui::Rect{x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
    }
    glEnable(GL_SCISSOR_TEST);
    const GLint sx = (GLint)std::floor(r.x);
    const GLint sy = (GLint)std::floor(mDispH - (r.y + r.h)); // GL-Y flippen
    glScissor(sx, sy, (GLsizei)std::ceil(r.w), (GLsizei)std::ceil(r.h));
}

void RuiGlTarget::ClipPush(const rui::Rect& r) {
    Flush();
    mClipStack.push_back(r);
    ApplyClip();
}

void RuiGlTarget::ClipPop() {
    Flush();
    if (!mClipStack.empty()) mClipStack.pop_back();
    ApplyClip();
}

} // namespace rpg
