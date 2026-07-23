#include "rpgmaker3d/RgssUI.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Shader.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Font8x8.h"

#include <glad/gl.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace rpg {

// Projekt-Basispfad (vom Engine beim Projekt-Laden gesetzt)
namespace {
std::string s_ProjectBase;
}

// ---------------------------------------------------------------------------
// OverlayRenderer: kleiner GL-Quad-Renderer (texturiert + eingefaerbt)
// ---------------------------------------------------------------------------
namespace {

const char* kOverlayVert = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;   // logischer 640x480-Raum
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;                // (640, 480)
out vec2 vUV;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0,
                    1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)GLSL";

const char* kOverlayFrag = R"GLSL(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
uniform int uUseTex;                 // 0 = nur Farbe, 1 = Textur * Farbe
out vec4 FragColor;
void main() {
    vec4 c = (uUseTex == 1) ? texture(uTex, vUV) : vec4(1.0);
    FragColor = c * vColor;
    if (FragColor.a < 0.004) discard;
}
)GLSL";

} // anonymous namespace

struct RgssUI::OverlayRenderer {
    struct Vertex { float x, y, u, v, r, g, b, a; };

    std::unique_ptr<Shader> shader;
    GLuint vao = 0, vbo = 0;
    std::vector<Vertex> verts;
    int screenW = 640, screenH = 480;

    bool Init() {
        shader = std::make_unique<Shader>();
        if (!shader->LoadFromSource(kOverlayVert, kOverlayFrag)) {
            RPG_LOG_ERROR("RGSS-Overlay: Shader konnte nicht gebaut werden");
            return false;
        }
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 65536, nullptr, GL_STREAM_DRAW);
        const GLsizei stride = (GLsizei)sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, u));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, r));
        glBindVertexArray(0);
        return true;
    }

    void Begin(int w, int h) {
        screenW = 640; screenH = 480; (void)w; (void)h;
        verts.clear();
        verts.reserve(1024);
    }

    void Quad(float x, float y, float w, float h,
              float u0, float v0, float u1, float v1,
              float r, float g, float b, float a) {
        if (w <= 0.0f || h <= 0.0f || a <= 0.0f) return;
        const Vertex tl{ x,     y,     u0, v0, r, g, b, a };
        const Vertex tr{ x + w, y,     u1, v0, r, g, b, a };
        const Vertex br{ x + w, y + h, u1, v1, r, g, b, a };
        const Vertex bl{ x,     y + h, u0, v1, r, g, b, a };
        verts.push_back(tl); verts.push_back(tr); verts.push_back(br);
        verts.push_back(tl); verts.push_back(br); verts.push_back(bl);
    }

    void Flush(GLuint texId, bool useTex) {
        if (verts.empty()) return;
        const size_t maxVerts = 65536;
        shader->Bind();
        shader->SetVec2("uScreen", Vec2((float)screenW, (float)screenH));
        shader->SetInt("uTex", 0);
        shader->SetInt("uUseTex", useTex ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        if (texId) glBindTexture(GL_TEXTURE_2D, texId);
        else glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(vao);
        size_t offset = 0;
        while (offset < verts.size()) {
            const size_t count = std::min(maxVerts, verts.size() - offset);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            (GLsizeiptr)(count * sizeof(Vertex)),
                            verts.data() + offset);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)count);
            offset += count;
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        verts.clear();
    }
};

// ---------------------------------------------------------------------------
// Verwaltung
// ---------------------------------------------------------------------------
RgssUI::RgssUI() = default;
RgssUI::~RgssUI() = default;

RgssUI& RgssUI::Get() {
    static RgssUI inst;
    return inst;
}

int RgssUI::CreateWindow(float x, float y, float w, float h) {
    RgssWindowState wnd;
    wnd.id = mNextId++;
    wnd.x = x; wnd.y = y; wnd.width = w; wnd.height = h;
    mWindows.push_back(wnd);
    return wnd.id;
}

void RgssUI::DisposeWindow(int id) {
    if (auto* w = GetWindow(id)) w->disposed = true;
}

RgssWindowState* RgssUI::GetWindow(int id) {
    for (auto& w : mWindows)
        if (w.id == id && !w.disposed) return &w;
    return nullptr;
}

void RgssUI::ClearAll() {
    mWindows.clear();
    mNextId = 1;
}

void RgssUI::SetProjectBase(const std::string& basePath) {
    if (s_ProjectBase == basePath) return;
    s_ProjectBase = basePath;
    // Skin-Cache verwerfen (Pfade koennten jetzt anders aufloesen).
    // GL-Texturen der alten Skins laufen mit dem shared_ptr sauber aus.
    Get().mSkinCache.clear();
}

const std::string& RgssUI::ProjectBase() {
    return s_ProjectBase;
}

void RgssUI::EnsureRenderer() {
    if (mRenderer) return;
    auto r = std::make_unique<OverlayRenderer>();
    if (r->Init()) mRenderer = std::move(r);
}

// ---------------------------------------------------------------------------
// Windowskin laden (lazy, Cache; Resolver wie bei den Pictures)
// ---------------------------------------------------------------------------
unsigned int RgssUI::SkinTexture(const std::string& skinName) {
    const std::string key = skinName.empty() ? std::string("__default__") : skinName;
    auto it = mSkinCache.find(key);
    if (it != mSkinCache.end())
        return it->second.texture ? it->second.texture->GetID() : 0;

    std::string name = skinName;
    if (name.empty()) name = Database::Get().System().windowskinName;
    const std::string base = s_ProjectBase.empty() ? std::string(".") : s_ProjectBase;
    static const char* kDirs[] = {
        "/Graphics/System/", "/Graphics/Windowskins/", "/Graphics/", "/"
    };
    std::string path;
    for (const char* d : kDirs) {
        const std::string cand = base + d + name + ".png";
        if (std::filesystem::exists(cand)) { path = cand; break; }
    }

    SkinEntry entry;
    if (!path.empty()) {
        entry.texture = std::make_shared<Texture>();
        if (!entry.texture->LoadFromFile(path)) {
            RPG_LOG_WARN("RGSS: Windowskin nicht ladbar: " + path);
            entry.texture.reset();
        }
    } else {
        RPG_LOG_WARN("RGSS: Windowskin nicht gefunden: " + name);
    }
    unsigned int id = entry.texture ? entry.texture->GetID() : 0;
    mSkinCache[key] = entry;
    return id;
}

// ---------------------------------------------------------------------------
// 8x8-Font-Atlas (144 Glyphen: 128 basis + deutsche Erweiterungen ab 128)
// ---------------------------------------------------------------------------
unsigned int RgssUI::FontAtlasTexture() {
    if (mFontTextureId) return mFontTextureId;
    const int glyphs = 144;
    const int w = 8, h = glyphs * 8;
    std::vector<unsigned char> px((size_t)w * h * 4, 0);

    // Basis-Glyphen kopieren
    for (int g = 0; g < 128; ++g)
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col) {
                const bool on = (kFont8x8Basic[g][row] >> (7 - col)) & 1;
                const size_t o = (((size_t)g * 8 + row) * 8 + col) * 4;
                px[o] = px[o + 1] = px[o + 2] = 255;
                px[o + 3] = on ? 255 : 0;
            }
    // Diaerese-Punkte auf deckendem Glyphen setzen (x = 2 und 5, Zeilen 0..1)
    const auto withDots = [&](int dst, int src, bool shiftDown) {
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col) {
                int srcRow = shiftDown ? row - 1 : row;
                if (srcRow < 0) srcRow = 0;
                const bool on = (kFont8x8Basic[src][srcRow] >> (7 - col)) & 1;
                const size_t o = (((size_t)dst * 8 + row) * 8 + col) * 4;
                px[o] = px[o + 1] = px[o + 2] = 255;
                px[o + 3] = on ? 255 : 0;
            }
        const int dotX[2] = { 2, 5 };
        for (int dx : dotX) {
            const size_t o0 = (((size_t)dst * 8 + 0) * 8 + dx) * 4;
            px[o0] = px[o0 + 1] = px[o0 + 2] = 255; px[o0 + 3] = 255;
        }
    };
    withDots(128, 'a', false); // ae
    withDots(129, 'o', false); // oe
    withDots(130, 'u', false); // ue
    withDots(131, 'A', true);  // AE
    withDots(132, 'O', true);  // OE
    withDots(133, 'U', true);  // UE
    // 134 = sz (aus s + Haken unten, Naeherung)
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col) {
            const bool on = (kFont8x8Basic['s'][row] >> (7 - col)) & 1;
            const size_t o = (((size_t)134 * 8 + row) * 8 + col) * 4;
            px[o] = px[o + 1] = px[o + 2] = 255;
            px[o + 3] = on ? 255 : 0;
        }
    // 135 bleibt leer (Platzhalter)

    glGenTextures(1, &mFontTextureId);
    glBindTexture(GL_TEXTURE_2D, mFontTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    mFontAtlasHeight = h;
    return mFontTextureId;
}

// ---------------------------------------------------------------------------
// Text decodieren (UTF-8 -> Glyph-Index)
// ---------------------------------------------------------------------------
static int RgssGlyphIndex(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { ++i; return c; }
    // UTF-8 2-Byte (deutsche Umlaute)
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        const unsigned int cp = ((c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
        i += 2;
        switch (cp) {
            case 0xE4: return 128; // ae
            case 0xF6: return 129; // oe
            case 0xFC: return 130; // ue
            case 0xC4: return 131; // AE
            case 0xD6: return 132; // OE
            case 0xDC: return 133; // UE
            case 0xDF: return 134; // sz
            default:   return '?';
        }
    }
    // Laengere Sequenzen ueberspringen
    int adv = 1;
    if ((c & 0xF0) == 0xE0) adv = 3;
    else if ((c & 0xF8) == 0xF0) adv = 4;
    i += (i + (size_t)adv <= s.size()) ? (size_t)adv : 1;
    return '?';
}

// ---------------------------------------------------------------------------
// Fenster zeichnen (9-Slice XP-Windowskin + Text)
// ---------------------------------------------------------------------------
void RgssUI::DrawWindow(const RgssWindowState& w) {
    if (!mRenderer) return;
    auto* r = mRenderer.get();

    // openness: oeffnet vertikal von der Mitte (RGSS-Look)
    const float op = std::clamp(w.openness, 0.0f, 1.0f);
    const float hh = w.height * op;
    const float y0 = w.y + (w.height - hh) * 0.5f;
    const float x0 = w.x, ww = w.width;
    if (ww <= 4.0f || hh <= 4.0f) return;

    // --- Windowskin (9-Slice) ---
    const unsigned int skinTex = SkinTexture(w.windowskin);
    int skinW = 0, skinH = 0;
    if (auto it = mSkinCache.find(w.windowskin.empty() ? "__default__" : w.windowskin);
        it != mSkinCache.end() && it->second.texture) {
        skinW = it->second.texture->GetWidth();
        skinH = it->second.texture->GetHeight();
    }
    if (skinTex && skinW > 0 && skinH > 0) {
        const float frameW = std::min(128.0f, (float)skinW); // linker Frame-Block
        const float c = std::min(16.0f, std::min(ww, hh) * 0.5f); // Ecke
        const float fw = frameW, fh = std::min(128.0f, (float)skinH);
        const auto uvx = [&](float px) { return px / (float)skinW; };
        const auto uvy = [&](float px) { return px / (float)skinH; };
        const float a = 1.0f;
        // Hintergrund (gestreckt aus dem mittleren Bereich des Frames)
        r->Quad(x0 + c, y0 + c, ww - 2 * c, hh - 2 * c,
                uvx(32), uvy(32), uvx(fw - 32), uvy(fh - 32), 1, 1, 1, a);
        // Ecken
        r->Quad(x0, y0, c, c, uvx(0), uvy(0), uvx(c), uvy(c), 1, 1, 1, a);
        r->Quad(x0 + ww - c, y0, c, c, uvx(fw - c), uvy(0), uvx(fw), uvy(c), 1, 1, 1, a);
        r->Quad(x0, y0 + hh - c, c, c, uvx(0), uvy(fh - c), uvx(c), uvy(fh), 1, 1, 1, a);
        r->Quad(x0 + ww - c, y0 + hh - c, c, c, uvx(fw - c), uvy(fh - c), uvx(fw), uvy(fh), 1, 1, 1, a);
        // Kanten (gestreckt)
        r->Quad(x0 + c, y0, ww - 2 * c, c, uvx(c), uvy(0), uvx(fw - c), uvy(c), 1, 1, 1, a);
        r->Quad(x0 + c, y0 + hh - c, ww - 2 * c, c, uvx(c), uvy(fh - c), uvx(fw - c), uvy(fh), 1, 1, 1, a);
        r->Quad(x0, y0 + c, c, hh - 2 * c, uvx(0), uvy(c), uvx(c), uvy(fh - c), 1, 1, 1, a);
        r->Quad(x0 + ww - c, y0 + c, c, hh - 2 * c, uvx(fw - c), uvy(c), uvx(fw), uvy(fh - c), 1, 1, 1, a);
        r->Flush(skinTex, true);
    } else {
        // Fallback ohne Grafik: dunkles Panel + heller 2px-Rahmen
        r->Quad(x0, y0, ww, hh, 0, 0, 0, 0, 0.1f, 0.1f, 0.25f, 0.85f);
        r->Quad(x0, y0, ww, 2, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, 1.0f);
        r->Quad(x0, y0 + hh - 2, ww, 2, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, 1.0f);
        r->Quad(x0, y0, 2, hh, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, 1.0f);
        r->Quad(x0 + ww - 2, y0, 2, hh, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, 1.0f);
        r->Flush(0, false);
    }

    // --- Text (8x8-Font, 2x skaliert = XP-artige 16px) ---
    if (!w.text.empty()) {
        const float scale = 2.0f, cw = 8 * scale, lh = 9 * scale;
        float tx = x0 + 16.0f, ty = y0 + 16.0f;
        const unsigned int fontTex = FontAtlasTexture();
        size_t i = 0;
        const size_t n = w.text.size();
        while (i < n) {
            const char ch = w.text[i];
            if (ch == '\n') { tx = x0 + 16.0f; ty += lh; ++i; continue; }
            const int g = RgssGlyphIndex(w.text, i);
            const float v0 = (float)(g * 8) / (float)mFontAtlasHeight;
            const float v1 = (float)((g + 1) * 8) / (float)mFontAtlasHeight;
            r->Quad(tx, ty, cw, cw, 0.0f, v0, 1.0f, v1,
                    w.textRed, w.textGreen, w.textBlue, w.textAlpha);
            tx += cw;
            if (tx + cw > x0 + ww - 16.0f) { tx = x0 + 16.0f; ty += lh; }
        }
        r->Flush(fontTex, true);
    }
}

void RgssUI::Render(int screenWidth, int screenHeight) {
    if (mWindows.empty()) return;
    EnsureRenderer();
    if (!mRenderer) return;

    // Aufraeumen: disposed-Fenster entfernen
    mWindows.erase(std::remove_if(mWindows.begin(), mWindows.end(),
        [](const RgssWindowState& w) { return w.disposed; }), mWindows.end());
    if (mWindows.empty()) return;

    // z-Ordnung (stabiles Sortieren nach z)
    std::vector<const RgssWindowState*> drawList;
    drawList.reserve(mWindows.size());
    for (const auto& w : mWindows) if (w.visible) drawList.push_back(&w);
    std::stable_sort(drawList.begin(), drawList.end(),
        [](const RgssWindowState* a, const RgssWindowState* b) { return a->z < b->z; });
    if (drawList.empty()) return;

    // GL-State fuer UI-Overlay: voller Viewport, kein Depth-Test, Alpha-Blend
    glViewport(0, 0, screenWidth, screenHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mRenderer->Begin(screenWidth, screenHeight);
    for (const auto* w : drawList) DrawWindow(*w);

    // State grob restaurieren (3D-Pass korrigiert ohnedies selbst)
    glEnable(GL_DEPTH_TEST);
}

} // namespace rpg
