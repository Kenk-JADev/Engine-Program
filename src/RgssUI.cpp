// ============================================================================
// RGSS-Laufzeit (RPG Maker XP-kompatibel) - Implementierung
// ----------------------------------------------------------------------------
// Aufbau:
//   1) Registries (Rect/Color/Tone/Font, Table, Bitmap, Viewport, Drawable)
//   2) OverlayRenderer (GL-Batch + Color/Tone/Blend-Shader + Scissor-Clipping)
//   3) Bitmap-Pixeloperationen (fill/blt/draw_text/hue/blur/radial_blur)
//   4) Drawable-Renderer (Sprite/Plane/Tilemap mit XP-Autotile-Tabelle)
//   5) Window-Renderer (volles XP-Windowskin 192x128)
//   6) Graphics-Overlay (freeze/transition) + Render-Hauptfunktion
// ============================================================================

#include "rpgmaker3d/RgssUI.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Shader.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Font8x8.h"

#include <glad/gl.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <unordered_map>

// MSVC definiert M_PI nur mit _USE_MATH_DEFINES -- robust selbst definieren.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rpg {

// Projekt-Basispfad (von der Engine beim Projekt-Laden gesetzt)
namespace {
std::string s_ProjectBase;
}

static const int kScreenW = 640;
static const int kScreenH = 480;

// ---------------------------------------------------------------------------
// 1) Registries
// ---------------------------------------------------------------------------
template <typename T>
struct RgssRegistry {
    std::unordered_map<int, std::unique_ptr<T>> items;
    int nextId = 1;
    int Add(std::unique_ptr<T> v) { const int id = nextId++; items[id] = std::move(v); return id; }
    T* Get(int id) {
        if (id <= 0) return nullptr;
        auto it = items.find(id);
        return it != items.end() ? it->second.get() : nullptr;
    }
    void Remove(int id) { items.erase(id); }
    void Clear() { items.clear(); nextId = 1; }
};

namespace {
RgssRegistry<RgssRectState>  s_rects;
RgssRegistry<RgssColorState> s_colors;
RgssRegistry<RgssToneState>  s_tones;
RgssRegistry<RgssFontState>  s_fonts;
RgssRegistry<RgssTableState> s_tables;
RgssRegistry<RgssBitmapState> s_bitmaps;
RgssRegistry<RgssViewportState> s_viewports;
RgssRegistry<RgssDrawableState> s_drawables;
std::unique_ptr<RgssFontState> s_fontDefaults;
RgssGraphicsState s_graphics;
std::uint64_t s_seqCounter = 1;
} // anonymous namespace

int RgssRectCreate(float x, float y, float w, float h) {
    auto v = std::make_unique<RgssRectState>();
    v->x = x; v->y = y; v->w = w; v->h = h;
    return s_rects.Add(std::move(v));
}
RgssRectState* RgssRectGet(int id) { return s_rects.Get(id); }

int RgssColorCreate(float r, float g, float b, float a) {
    auto v = std::make_unique<RgssColorState>();
    v->r = std::clamp(r, 0.0f, 255.0f); v->g = std::clamp(g, 0.0f, 255.0f);
    v->b = std::clamp(b, 0.0f, 255.0f); v->a = std::clamp(a, 0.0f, 255.0f);
    return s_colors.Add(std::move(v));
}
RgssColorState* RgssColorGet(int id) { return s_colors.Get(id); }

int RgssToneCreate(float r, float g, float b, float gray) {
    auto c = [](float v) { return std::clamp(v, -255.0f, 255.0f); };
    auto v = std::make_unique<RgssToneState>();
    v->r = c(r); v->g = c(g); v->b = c(b); v->gray = c(gray);
    return s_tones.Add(std::move(v));
}
RgssToneState* RgssToneGet(int id) { return s_tones.Get(id); }

RgssFontState& RgssFontDefaults() {
    if (!s_fontDefaults) {
        s_fontDefaults = std::make_unique<RgssFontState>();
        s_fontDefaults->colorId = RgssColorCreate(255, 255, 255, 255);
    }
    return *s_fontDefaults;
}
int RgssFontCreate() {
    auto v = std::make_unique<RgssFontState>(RgssFontDefaults());
    // Farbe bekommt eine eigene Kopie (Font-Objekte sind unabhaengig)
    if (auto* c = RgssColorGet(v->colorId))
        v->colorId = RgssColorCreate(c->r, c->g, c->b, c->a);
    return s_fonts.Add(std::move(v));
}
RgssFontState* RgssFontGet(int id) { return s_fonts.Get(id); }

int RgssTableCreate(int xs, int ys, int zs) {
    auto v = std::make_unique<RgssTableState>();
    v->xs = std::max(1, xs); v->ys = std::max(1, ys); v->zs = std::max(1, zs);
    v->data.assign((size_t)v->xs * v->ys * v->zs, 0);
    return s_tables.Add(std::move(v));
}
RgssTableState* RgssTableGet(int id) { return s_tables.Get(id); }

int RgssVpCreate(float x, float y, float w, float h) {
    auto v = std::make_unique<RgssViewportState>();
    v->x = x; v->y = y; v->w = w; v->h = h;
    return s_viewports.Add(std::move(v));
}
RgssViewportState* RgssVpGet(int id) { return s_viewports.Get(id); }
void RgssVpDispose(int id) {
    if (auto* v = s_viewports.Get(id)) v->disposed = true;
}

int RgssDrawableCreate(RgssDrawableType type, int viewportId) {
    auto v = std::make_unique<RgssDrawableState>();
    v->type = type;
    v->viewportId = viewportId;
    v->seq = s_seqCounter++;
    return s_drawables.Add(std::move(v));
}
RgssDrawableState* RgssDrawableGet(int id) { return s_drawables.Get(id); }
void RgssDrawableDispose(int id) {
    if (auto* v = s_drawables.Get(id)) v->disposed = true;
}

RgssGraphicsState& RgssGraphics() { return s_graphics; }

// ---------------------------------------------------------------------------
// Grafik-Aufloesung (relativer Projektpfad, Erweiterung darf fehlen)
// ---------------------------------------------------------------------------
std::string RgssResolveGraphic(const std::string& relPath) {
    if (relPath.empty()) return std::string();
    const std::string base = s_ProjectBase.empty() ? std::string(".") : s_ProjectBase;
    static const char* kExts[] = { ".png", ".jpg", ".jpeg", ".bmp" };

    std::string rel = relPath;
    // fuehrende Slashes entfernen (Pfade wie "/Graphics/...")
    while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) rel.erase(rel.begin());
    std::replace(rel.begin(), rel.end(), '\\', '/');

    const bool hasExt = rel.find('.') != std::string::npos;
    const std::string direct = base + "/" + rel;
    if (hasExt && std::filesystem::exists(direct)) return direct;
    if (!hasExt) {
        for (const char* e : kExts) {
            if (std::filesystem::exists(direct + e)) return direct + e;
        }
    }
    // Windowskin-Sonderfall: XP-RTP-Pfad Graphics/Windowskins -> auch
    // Graphics/System des Projekts akzeptieren (unser Editor legt Skins dort ab)
    size_t basePos = rel.rfind('/');
    const std::string fileOnly = basePos == std::string::npos ? rel : rel.substr(basePos + 1);
    static const char* kDirs[] = {
        "Graphics/System/", "Graphics/Windowskins/", "Graphics/Pictures/", "Graphics/"
    };
    for (const char* d : kDirs) {
        const std::string stem = base + "/" + d + fileOnly;
        for (const char* e : kExts) {
            if (std::filesystem::exists(stem + e)) return stem + e;
        }
        if (hasExt && std::filesystem::exists(stem)) return stem;
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Bitmap
// ---------------------------------------------------------------------------
int RgssBmpCreate(int w, int h) {
    if (w <= 0 || h <= 0) return 0;
    auto v = std::make_unique<RgssBitmapState>();
    v->width = w; v->height = h;
    v->pixels.assign((size_t)w * h * 4, 0);
    v->fontId = RgssFontCreate();
    return s_bitmaps.Add(std::move(v));
}

int RgssBmpLoad(const std::string& pathOrRel) {
    std::string path = std::filesystem::path(pathOrRel).is_absolute() ||
                       std::filesystem::exists(pathOrRel)
                           ? pathOrRel
                           : RgssResolveGraphic(pathOrRel);
    if (path.empty()) return 0;
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data) stbi_image_free(data);
        RPG_LOG_WARN("RGSS: Bild nicht ladbar: " + path);
        return 0;
    }
    auto v = std::make_unique<RgssBitmapState>();
    v->width = w; v->height = h;
    v->pixels.assign(data, data + (size_t)w * h * 4);
    stbi_image_free(data);
    v->fontId = RgssFontCreate();
    return s_bitmaps.Add(std::move(v));
}

int RgssBmpClone(int srcId) {
    auto* src = s_bitmaps.Get(srcId);
    if (!src || src->disposed) return 0;
    auto v = std::make_unique<RgssBitmapState>(*src); // kopiert Pixel
    v->textureId = 0;  // eigene GL-Textur
    v->dirty = true;
    v->disposed = false;
    return s_bitmaps.Add(std::move(v));
}

RgssBitmapState* RgssBmpGet(int id) { return s_bitmaps.Get(id); }

void RgssBmpDispose(int id) {
    auto* b = s_bitmaps.Get(id);
    if (!b || b->disposed) return;
    if (b->textureId) { glDeleteTextures(1, &b->textureId); b->textureId = 0; }
    b->pixels.clear();
    b->disposed = true;
}

static inline void BmpPut(RgssBitmapState* b, int x, int y,
                          float r, float g, float bl, float a, float srcMul) {
    if (x < 0 || y < 0 || x >= b->width || y >= b->height) return;
    uint8_t* p = b->pixels.data() + ((size_t)y * b->width + x) * 4;
    const float sa = std::clamp(a * srcMul, 0.0f, 255.0f) / 255.0f;
    const float da = p[3] / 255.0f;
    const float oa = sa + da * (1.0f - sa);
    if (oa <= 0.0f) { p[0] = p[1] = p[2] = p[3] = 0; return; }
    for (int c = 0; c < 3; ++c) {
        const float sc = c == 0 ? r : (c == 1 ? g : bl);
        p[c] = (uint8_t)std::clamp(
            (sc * sa + p[c] * da * (1.0f - sa)) / oa, 0.0f, 255.0f);
    }
    p[3] = (uint8_t)std::clamp(oa * 255.0f, 0.0f, 255.0f);
}

void RgssBmpFillRect(int id, float x, float y, float w, float h,
                     float r, float g, float b, float a) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    const int x1 = std::clamp((int)std::round(x), 0, bmp->width);
    const int y1 = std::clamp((int)std::round(y), 0, bmp->height);
    const int x2 = std::clamp((int)std::round(x + w), 0, bmp->width);
    const int y2 = std::clamp((int)std::round(y + h), 0, bmp->height);
    for (int py = y1; py < y2; ++py)
        for (int px = x1; px < x2; ++px)
            BmpPut(bmp, px, py, r, g, b, a, 1.0f);
    bmp->dirty = true;
}

void RgssBmpGradientFillRect(int id, float x, float y, float w, float h,
                             float r1, float g1, float b1, float a1,
                             float r2, float g2, float b2, float a2, bool vertical) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    const int x1 = std::clamp((int)std::round(x), 0, bmp->width);
    const int y1 = std::clamp((int)std::round(y), 0, bmp->height);
    const int x2 = std::clamp((int)std::round(x + w), 0, bmp->width);
    const int y2 = std::clamp((int)std::round(y + h), 0, bmp->height);
    for (int py = y1; py < y2; ++py)
        for (int px = x1; px < x2; ++px) {
            const float t = vertical
                ? (y2 > y1 ? (float)(py - y1) / (float)(y2 - y1 - 1) : 0.0f)
                : (x2 > x1 ? (float)(px - x1) / (float)(x2 - x1 - 1) : 0.0f);
            BmpPut(bmp, px, py,
                   r1 + (r2 - r1) * t, g1 + (g2 - g1) * t,
                   b1 + (b2 - b1) * t, a1 + (a2 - a1) * t, 1.0f);
        }
    bmp->dirty = true;
}

void RgssBmpClear(int id) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    std::fill(bmp->pixels.begin(), bmp->pixels.end(), (uint8_t)0);
    bmp->dirty = true;
}

void RgssBmpClearRect(int id, float x, float y, float w, float h) {
    RgssBmpFillRect(id, x, y, w, h, 0, 0, 0, 0);
}

void RgssBmpStretchBlt(int dstId, float dx, float dy, float dw, float dh,
                       int srcId, float sx, float sy, float sw, float sh, float opacity) {
    auto* dst = s_bitmaps.Get(dstId);
    auto* src = s_bitmaps.Get(srcId);
    if (!dst || !src || dst->disposed || src->disposed) return;
    if (dw == 0 || dh == 0 || sw == 0 || sh == 0) return;
    const float op = std::clamp(opacity, 0.0f, 255.0f) / 255.0f;
    if (op <= 0.0f) return;
    const int x1 = (int)std::round(dx), y1 = (int)std::round(dy);
    const int x2 = (int)std::round(dx + dw), y2 = (int)std::round(dy + dh);
    const float srcA = (float)src->height; // (Referenz-Groesse)
    (void)srcA;
    const int srcSX = (int)std::round(sx), srcSY = (int)std::round(sy);
    const int srcSW = (int)std::round(sw), srcSH = (int)std::round(sh);
    const int w = x2 - x1, h = y2 - y1;
    const int stepX = w > 0 ? 1 : -1, stepY = h > 0 ? 1 : -1;
    const int countX = std::abs(w), countY = std::abs(h);
    for (int iy = 0; iy < countY; ++iy) {
        const int ty = y1 + iy * stepY;
        const int tys = srcSY + (int)((int64_t)iy * srcSH / std::max(1, countY)) * (srcSH > 0 ? 1 : -1);
        for (int ix = 0; ix < countX; ++ix) {
            const int tx = x1 + ix * stepX;
            const int txs = srcSX + (int)((int64_t)ix * srcSW / std::max(1, countX)) * (srcSW > 0 ? 1 : -1);
            if (txs < 0 || tys < 0 || txs >= src->width || tys >= src->height) continue;
            const uint8_t* sp = src->pixels.data() + ((size_t)tys * src->width + txs) * 4;
            BmpPut(dst, tx, ty, (float)sp[0], (float)sp[1], (float)sp[2], (float)sp[3], op);
        }
    }
    dst->dirty = true;
}

void RgssBmpBlt(int dstId, float dx, float dy, int srcId,
                float sx, float sy, float sw, float sh, float opacity) {
    RgssBmpStretchBlt(dstId, dx, dy, sw, sh, srcId, sx, sy, sw, sh, opacity);
}

bool RgssBmpGetPixel(int id, int x, int y, float& r, float& g, float& b, float& a) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed || x < 0 || y < 0 || x >= bmp->width || y >= bmp->height)
        return false;
    const uint8_t* p = bmp->pixels.data() + ((size_t)y * bmp->width + x) * 4;
    r = (float)p[0]; g = (float)p[1]; b = (float)p[2]; a = (float)p[3];
    return true;
}

void RgssBmpSetPixel(int id, int x, int y, float r, float g, float b, float a) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    BmpPut(bmp, x, y, r, g, b, a, 1.0f);
    bmp->dirty = true;
}

void RgssBmpHueChange(int id, float hueDeg) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    const float hueShift = std::fmod(hueDeg, 360.0f) / 360.0f * 6.0f;
    const auto rgbToHsv = [](float r, float g, float b, float& h, float& s, float& v) {
        const float mx = std::max({ r, g, b }), mn = std::min({ r, g, b });
        const float d = mx - mn;
        v = mx;
        s = mx > 0.0f ? d / mx : 0.0f;
        if (d <= 0.0f) { h = 0.0f; return; }
        if (mx == r) h = std::fmod((g - b) / d, 6.0f);
        else if (mx == g) h = (b - r) / d + 2.0f;
        else h = (r - g) / d + 4.0f;
        if (h < 0.0f) h += 6.0f;
    };
    const auto hsvToRgb = [](float h, float s, float v, float& r, float& g, float& b) {
        h = std::fmod(h + 6.0f, 6.0f);
        const int i = (int)h;
        const float f = h - i;
        const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    };
    for (size_t i = 0; i + 3 < bmp->pixels.size(); i += 4) {
        if (bmp->pixels[i + 3] == 0) continue;
        float h, s, v;
        rgbToHsv(bmp->pixels[i] / 255.0f, bmp->pixels[i + 1] / 255.0f,
                 bmp->pixels[i + 2] / 255.0f, h, s, v);
        float r, g, b;
        hsvToRgb(h + hueShift, s, v, r, g, b);
        bmp->pixels[i] = (uint8_t)std::clamp(r * 255.0f, 0.0f, 255.0f);
        bmp->pixels[i + 1] = (uint8_t)std::clamp(g * 255.0f, 0.0f, 255.0f);
        bmp->pixels[i + 2] = (uint8_t)std::clamp(b * 255.0f, 0.0f, 255.0f);
    }
    bmp->dirty = true;
}

void RgssBmpBlur(int id) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    std::vector<uint8_t> tmp(bmp->pixels.size());
    const int R = 2;
    // horizontal
    for (int y = 0; y < bmp->height; ++y)
        for (int x = 0; x < bmp->width; ++x) {
            int ac[4] = { 0, 0, 0, 0 }, n = 0;
            for (int k = -R; k <= R; ++k) {
                const int xx = std::clamp(x + k, 0, bmp->width - 1);
                const uint8_t* p = bmp->pixels.data() + ((size_t)y * bmp->width + xx) * 4;
                ac[0] += p[0]; ac[1] += p[1]; ac[2] += p[2]; ac[3] += p[3]; ++n;
            }
            uint8_t* p = tmp.data() + ((size_t)y * bmp->width + x) * 4;
            for (int c = 0; c < 4; ++c) p[c] = (uint8_t)(ac[c] / n);
        }
    // vertikal
    for (int y = 0; y < bmp->height; ++y)
        for (int x = 0; x < bmp->width; ++x) {
            int ac[4] = { 0, 0, 0, 0 }, n = 0;
            for (int k = -R; k <= R; ++k) {
                const int yy = std::clamp(y + k, 0, bmp->height - 1);
                const uint8_t* p = tmp.data() + ((size_t)yy * bmp->width + x) * 4;
                ac[0] += p[0]; ac[1] += p[1]; ac[2] += p[2]; ac[3] += p[3]; ++n;
            }
            uint8_t* p = bmp->pixels.data() + ((size_t)y * bmp->width + x) * 4;
            for (int c = 0; c < 4; ++c) p[c] = (uint8_t)(ac[c] / n);
        }
    bmp->dirty = true;
}

void RgssBmpRadialBlur(int id, float angleDeg, int division) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    division = std::clamp(division, 2, 100);
    const std::vector<uint8_t> orig = bmp->pixels;
    const float cx = bmp->width / 2.0f, cy = bmp->height / 2.0f;
    const float total = angleDeg * (float)M_PI / 180.0f;
    for (int y = 0; y < bmp->height; ++y)
        for (int x = 0; x < bmp->width; ++x) {
            int ac[4] = { 0, 0, 0, 0 };
            int weight = 0;
            for (int d = 0; d < division; ++d) {
                const float t = (division > 1) ? (float)d / (float)(division - 1) : 0.0f;
                const float ang = -total * 0.5f + total * t;
                const float cs = std::cos(ang), sn = std::sin(ang);
                const float px = x - cx, py = y - cy;
                const int sx = std::clamp((int)std::lround(cx + px * cs - py * sn), 0, bmp->width - 1);
                const int sy = std::clamp((int)std::lround(cy + px * sn + py * cs), 0, bmp->height - 1);
                const uint8_t* p = orig.data() + ((size_t)sy * bmp->width + sx) * 4;
                for (int c = 0; c < 4; ++c) ac[c] += p[c];
                ++weight;
            }
            uint8_t* p = bmp->pixels.data() + ((size_t)y * bmp->width + x) * 4;
            for (int c = 0; c < 4; ++c) p[c] = (uint8_t)(ac[c] / weight);
        }
    bmp->dirty = true;
}

// ---------------------------------------------------------------------------
// Glyph-Raster (8x8, Daniel-Hepper-Basis + deutsche Extras) fuer draw_text
// ---------------------------------------------------------------------------
namespace {
uint8_t g_glyphBits[160][8];
bool g_glyphReady = false;

void FillGlyphBits() {
    if (g_glyphReady) return;
    std::memset(g_glyphBits, 0, sizeof(g_glyphBits));
    for (int g = 0; g < 128; ++g)
        for (int row = 0; row < 8; ++row)
            g_glyphBits[g][row] = kFont8x8Basic[g][row];
    const auto withDots = [&](int dst, int src, bool shiftDown) {
        for (int row = 0; row < 8; ++row) {
            int srcRow = shiftDown ? row - 1 : row;
            if (srcRow < 0) srcRow = 0;
            uint8_t bits = kFont8x8Basic[src][srcRow];
            bits |= (uint8_t)((1 << (7 - 2)) | (1 << (7 - 5))); // Diaerese x=2,5 Zeile 0
            if (row == 0) g_glyphBits[dst][row] = (uint8_t)((1 << (7 - 2)) | (1 << (7 - 5)));
            else if (row == 1 && !shiftDown) g_glyphBits[dst][row] = 0;
            else g_glyphBits[dst][row] = bits;
        }
        (void)0;
    };
    withDots(128, 'a', false);
    withDots(129, 'o', false);
    withDots(130, 'u', false);
    withDots(131, 'A', true);
    withDots(132, 'O', true);
    withDots(133, 'U', true);
    for (int row = 0; row < 8; ++row) g_glyphBits[134][row] = kFont8x8Basic['s'][row]; // sz ~ s
    g_glyphReady = true;
}

// UTF-8 -> Glyph-Index (identisch zur Fenster-Textdecodierung)
int RgssGlyphIndex(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { ++i; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        const unsigned int cp = ((c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
        i += 2;
        switch (cp) {
            case 0xE4: return 128;
            case 0xF6: return 129;
            case 0xFC: return 130;
            case 0xC4: return 131;
            case 0xD6: return 132;
            case 0xDC: return 133;
            case 0xDF: return 134;
            default:   return '?';
        }
    }
    int adv = 1;
    if ((c & 0xF0) == 0xE0) adv = 3;
    else if ((c & 0xF8) == 0xF0) adv = 4;
    i += (i + (size_t)adv <= s.size()) ? (size_t)adv : 1;
    return '?';
}

// Textpsalm: einzelnes Zeichen in Pixelpuffer malen
void DrawGlyphPx(RgssBitmapState* bmp, int x0, int y0, int glyph, float scale,
                 bool bold, bool italic, float r, float g, float b, float a) {
    if (glyph < 0 || glyph >= 160) glyph = '?';
    const float shear = italic ? 0.25f : 0.0f;
    const int gw = 8;
    const float gh = 8.0f * scale;
    for (float py = 0.0f; py < gh; py += 1.0f) {
        const int row = std::clamp((int)(py / scale), 0, 7);
        const int shearx = italic ? (int)std::lround((gh - py) * shear) : 0;
        for (float px = 0; px < (float)gw * scale; px += 1.0f) {
            const int col = (int)(px / scale);
            if (col > 7) break;
            const bool on = (g_glyphBits[glyph][row] >> (7 - col)) & 1;
            if (!on) continue;
            const int dx = x0 + (int)px + shearx;
            const int dy = y0 + (int)py;
            BmpPut(bmp, dx, dy, r, g, b, a, 1.0f);
            if (bold) BmpPut(bmp, dx + 1, dy, r, g, b, a, 1.0f);
        }
    }
}

float MeasureTextWidth(const std::string& utf8, float scale) {
    size_t i = 0;
    float w = 0;
    while (i < utf8.size()) {
        if (utf8[i] == '\n') break;
        RgssGlyphIndex(utf8, i);
        w += 8.0f * scale;
    }
    return w;
}
} // anonymous namespace

void RgssBmpDrawText(int id, float x, float y, float w, float h,
                     const std::string& utf8, int align) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) return;
    FillGlyphBits();
    const RgssFontState* font = RgssFontGet(bmp->fontId);
    RgssFontState localFont;
    if (!font) font = &localFont;
    const RgssColorState* col = RgssColorGet(font->colorId);
    const float cr = col ? col->r : 255, cg = col ? col->g : 255;
    const float cb = col ? col->b : 255, ca = col ? col->a : 255;

    float scale = std::max(0.5f, (float)font->size / 8.0f);
    float textW = MeasureTextWidth(utf8, scale);
    // RGSS: Text wird bis auf 60% gestaucht, wenn er die Box sprengt
    if (textW > w && textW > 0.0f) {
        const float fit = std::max(0.6f, w / textW);
        scale *= fit;
        textW = MeasureTextWidth(utf8, scale);
    }
    const float lineH = 9.0f * scale;
    float ty = y + (h - lineH) * 0.5f; // RGSS: vertikal zentriert
    size_t i = 0;
    while (i < utf8.size()) {
        const size_t lineStart = i;
        float lineW = MeasureTextWidth(utf8.substr(0), scale); // volle Breite ab Start
        // Zeilenbreite bestimmen
        {
            size_t j = lineStart;
            lineW = 0;
            while (j < utf8.size() && utf8[j] != '\n') { RgssGlyphIndex(utf8, j); lineW += 8.0f * scale; }
        }
        float tx = x;
        if (align == 1) tx = x + (w - lineW) * 0.5f;
        else if (align == 2) tx = x + w - lineW;
        while (i < utf8.size() && utf8[i] != '\n') {
            size_t before = i;
            const int g = RgssGlyphIndex(utf8, i);
            (void)before;
            // PAKET 13: XP Font#shadow — 1px versetzte, halbtransparent
            // schwarze Kopie UNTER dem Zeichen (XP: Color(0,0,0,128)).
            // Schatten-Alpha deckelt an der Zeichen-Deckkraft (ca).
            if (font->shadow) {
                const float sa = ca < 128.0f ? ca : 128.0f;
                DrawGlyphPx(bmp, (int)std::lround(tx) + 1, (int)std::lround(ty) + 1,
                            g, scale, font->bold, font->italic,
                            0.0f, 0.0f, 0.0f, sa);
            }
            DrawGlyphPx(bmp, (int)std::lround(tx), (int)std::lround(ty), g, scale,
                        font->bold, font->italic, cr, cg, cb, ca);
            tx += 8.0f * scale;
        }
        if (i < utf8.size() && utf8[i] == '\n') { ++i; ty += lineH; }
    }
    bmp->dirty = true;
}

void RgssBmpTextSize(int id, const std::string& utf8, float& w, float& h) {
    auto* bmp = s_bitmaps.Get(id);
    if (!bmp || bmp->disposed) { w = h = 0; return; }
    const RgssFontState* font = RgssFontGet(bmp->fontId);
    const float scale = std::max(0.5f, (float)(font ? font->size : 22) / 8.0f);
    // laengste Zeile
    size_t i = 0;
    float maxW = 0;
    int lines = 1;
    while (i < utf8.size()) {
        float lw = 0;
        while (i < utf8.size() && utf8[i] != '\n') { RgssGlyphIndex(utf8, i); lw += 8.0f * scale; }
        maxW = std::max(maxW, lw);
        if (i < utf8.size() && utf8[i] == '\n') { ++i; ++lines; }
    }
    w = maxW;
    h = 9.0f * scale * lines;
}

void RgssBmpDisposeAll() {}

// ---------------------------------------------------------------------------
// 2) OverlayRenderer
// ---------------------------------------------------------------------------
namespace {

const char* kOverlayVert = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;   // logischer 640x480-Raum
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;
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
uniform int   uUseTex;       // 0 = nur Farbe, 1 = Textur * Farbe
uniform vec4  uBlendColor;   // RGB + Staerke in a (Color/Flash-Mischung)
uniform vec3  uToneAdd;      // Tonverschiebung (-1..1)
uniform float uToneGray;     // Graustufenanteil 0..1
out vec4 FragColor;
void main() {
    vec4 c = (uUseTex == 1) ? texture(uTex, vUV) : vec4(1.0);
    c = c * vColor;
    if (uToneGray > 0.001) {
        float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));
        c.rgb = mix(c.rgb, vec3(g), uToneGray);
    }
    c.rgb = clamp(c.rgb + uToneAdd, 0.0, 1.0);
    if (uBlendColor.a > 0.001)
        c.rgb = mix(c.rgb, uBlendColor.rgb, uBlendColor.a);
    FragColor = c;
    if (FragColor.a < 0.004) discard;
}
)GLSL";

// Masken-Transition: altes Bild (Snapshot) + Graustufenmaske aus Grafikdatei
const char* kTransFrag = R"GLSL(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
uniform sampler2D uMask;
uniform float uProg;    // 0 -> altes Bild sichtbar, 1 -> weg
uniform float uVague;   // Randunschaerfe 0..1
uniform int   uUseMask; // 0 = Crossfade
out vec4 FragColor;
void main() {
    vec4 c = texture(uTex, vUV);
    float a;
    if (uUseMask == 1) {
        float lum = texture(uMask, vUV).r;
        float vg = max(uVague, 0.0001) * 0.5;
        a = clamp((lum - (1.0 - uProg) * (1.0 + vg) + vg) / vg, 0.0, 1.0);
    } else {
        a = 1.0 - uProg;
    }
    FragColor = vec4(c.rgb, c.a * a);
    if (FragColor.a < 0.004) discard;
}
)GLSL";

} // anonymous namespace

struct RgssUI::OverlayRenderer {
    struct Vertex { float x, y, u, v, r, g, b, a; };

    std::unique_ptr<Shader> shader;
    std::unique_ptr<Shader> transShader;
    GLuint vao = 0, vbo = 0;
    std::vector<Vertex> verts;
    // Viewport-Bereich im Framebuffer, auf den der 640x480-Canvas abgebildet
    // wird (RPG-Maker-Seitenverhaeltnis 4:3, zentriert — kein Verzerren
    // mehr auf 16:9-Fenster). Wird von RgssUI::Render via Begin gesetzt.
    int viewX = 0, viewY = 0, viewW = 640, viewH = 480;

    // aktueller Batch-Zustand
    GLuint curTex = 0;
    bool curUseTex = false;
    int curBlend = 0;
    float blendR = 0, blendG = 0, blendB = 0, blendA = 0; // 0..1
    float toneR = 0, toneG = 0, toneB = 0, toneGray = 0;  // -1..1 / 0..1
    bool clipOn = false;
    int clipX = 0, clipY = 0, clipW = 0, clipH = 0;
    bool drawing = false;

    bool Init() {
        shader = std::make_unique<Shader>();
        if (!shader->LoadFromSource(kOverlayVert, kOverlayFrag)) {
            RPG_LOG_ERROR("RGSS-Overlay: Shader konnte nicht gebaut werden");
            return false;
        }
        transShader = std::make_unique<Shader>();
        if (!transShader->LoadFromSource(kOverlayVert, kTransFrag)) {
            RPG_LOG_WARN("RGSS-Overlay: Transitions-Shader fehlgeschlagen (Crossfade-Fallback)");
            transShader.reset();
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

    void Begin(int viewportX, int viewportY, int viewportW, int viewportH) {
        viewX = viewportX; viewY = viewportY;
        viewW = viewportW; viewH = viewportH;
        verts.clear();
        drawing = true;
        curTex = 0; curUseTex = false; curBlend = 0;
        blendR = blendG = blendB = blendA = 0;
        toneR = toneG = toneB = toneGray = 0;
        clipOn = false;
    }

    // Fuer Shader-Uniform-Reihenfolge in 0..1: aussen 0..255 -> hier /255
    void SetState(GLuint tex, bool useTex, int blendMode,
                  const float* blendCol255,   // [4] oder nullptr
                  const float* toneAdd255,    // [4] (rgb + gray) oder nullptr
                  float opacity255) {
        SetStateRaw(tex, useTex, blendMode,
                    blendCol255, toneAdd255, opacity255);
    }
    void SetStateRaw(GLuint tex, bool useTex, int blendMode,
                     const float* blendCol255, const float* toneAdd255,
                     float /*opacity*/) {
        Flush();
        curTex = tex; curUseTex = useTex; curBlend = blendMode;
        if (blendCol255) {
            blendR = blendCol255[0] / 255.0f; blendG = blendCol255[1] / 255.0f;
            blendB = blendCol255[2] / 255.0f; blendA = blendCol255[3] / 255.0f;
        } else blendR = blendG = blendB = blendA = 0;
        if (toneAdd255) {
            toneR = toneAdd255[0] / 255.0f; toneG = toneAdd255[1] / 255.0f;
            toneB = toneAdd255[2] / 255.0f; toneGray = std::clamp(toneAdd255[3] / 255.0f, 0.0f, 1.0f);
        } else toneR = toneG = toneB = toneGray = 0;
    }

    // Clip in logischen 640x480-Koordinaten (nullptr => Clip aus)
    void SetClip(const float* rectLogical) {
        Flush();
        if (!rectLogical) {
            if (clipOn) { glDisable(GL_SCISSOR_TEST); clipOn = false; }
            return;
        }
        // Clip in Canvas-Koordinaten -> Framebuffer-Ausschnitt des
        // 4:3-Viewports (viewX/viewY-Offset: Scissor ist framebuffer-absolut)
        const float sx = viewX + rectLogical[0] / (float)kScreenW * viewW;
        const float sy = viewY + (1.0f - (rectLogical[1] + rectLogical[3]) / (float)kScreenH) * viewH;
        const float sw = rectLogical[2] / (float)kScreenW * viewW;
        const float sh = rectLogical[3] / (float)kScreenH * viewH;
        clipOn = true;
        glEnable(GL_SCISSOR_TEST);
        glScissor((GLint)std::floor(sx), (GLint)std::floor(sy),
                  (GLsizei)std::max(1.0f, std::ceil(sw)),
                  (GLsizei)std::max(1.0f, std::ceil(sh)));
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

    // 4-Eck-Farben (Reihenfolge: oben-links, oben-rechts, unten-rechts, unten-links)
    void Quad4(float x, float y, float w, float h,
               float u0, float v0, float u1, float v1,
               const float col[4][3], float a0, float a1, float a2, float a3) {
        if (w <= 0.0f || h <= 0.0f) return;
        const Vertex tl{ x,     y,     u0, v0, col[0][0], col[0][1], col[0][2], a0 };
        const Vertex tr{ x + w, y,     u1, v0, col[1][0], col[1][1], col[1][2], a1 };
        const Vertex br{ x + w, y + h, u1, v1, col[2][0], col[2][1], col[2][2], a2 };
        const Vertex bl{ x,     y + h, u0, v1, col[3][0], col[3][1], col[3][2], a3 };
        verts.push_back(tl); verts.push_back(tr); verts.push_back(br);
        verts.push_back(tl); verts.push_back(br); verts.push_back(bl);
    }

    // Rotiert um Pivot (px,py), Winkel in Grad (XP: gegen Uhrzeiger)
    void QuadRot(float x, float y, float w, float h, float px, float py, float angleDeg,
                 float u0, float v0, float u1, float v1,
                 float r, float g, float b, float a, float bushDepth) {
        const float ang = angleDeg * (float)M_PI / 180.0f; // y-down: positiv = CW
        const float cs = std::cos(ang), sn = std::sin(ang);
        const auto rot = [&](float lx, float ly, float& ox, float& oy) {
            const float dx = lx - px, dy = ly - py;
            ox = px + dx * cs - dy * sn;
            oy = py + dx * sn + dy * cs;
        };
        float xs[4], ys[4];
        rot(x, y, xs[0], ys[0]);
        rot(x + w, y, xs[1], ys[1]);
        rot(x + w, y + h, xs[2], ys[2]);
        rot(x, y + h, xs[3], ys[3]);
        const Vertex tl{ xs[0], ys[0], u0, v0, r, g, b, a };
        const Vertex tr{ xs[1], ys[1], u1, v0, r, g, b, a };
        const Vertex br{ xs[2], ys[2], u1, v1, r, g, b, a };
        const Vertex bl{ xs[3], ys[3], u0, v1, r, g, b, a };
        verts.push_back(tl); verts.push_back(tr); verts.push_back(br);
        verts.push_back(tl); verts.push_back(br); verts.push_back(bl);
        (void)bushDepth; // Bush in der rotierten Variante vernachlaessigt
    }

    void Flush() {
        if (verts.empty()) return;
        const size_t maxVerts = 65536;
        shader->Bind();
        shader->SetVec2("uScreen", Vec2((float)kScreenW, (float)kScreenH));
        shader->SetInt("uTex", 0);
        shader->SetInt("uUseTex", curUseTex ? 1 : 0);
        shader->SetVec4("uBlendColor", Vec4(blendR, blendG, blendB, blendA));
        shader->SetVec3("uToneAdd", Vec3(toneR, toneG, toneB));
        shader->SetFloat("uToneGray", toneGray);
        if (curBlend == 1) {
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else if (curBlend == 2) {
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glActiveTexture(GL_TEXTURE0);
        if (curTex) glBindTexture(GL_TEXTURE_2D, curTex);
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
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        verts.clear();
    }

    // Transition-Vollbild (Snapshot-Textur, optional mit Maskentextur)
    void DrawTransition(GLuint snapshotTex, GLuint maskTex, float prog, float vague, bool useMask) {
        Flush();
        if (!snapshotTex || !transShader) {
            // Fallback: einfacher Crossfade ohne Maske
            SetState(snapshotTex, true, 0, nullptr, nullptr, 255);
            Quad(0, 0, (float)kScreenW, (float)kScreenH, 0, 0, 1, 1, 1, 1, 1, 1.0f - prog);
            Flush();
            return;
        }
        transShader->Bind();
        transShader->SetVec2("uScreen", Vec2((float)kScreenW, (float)kScreenH));
        transShader->SetInt("uTex", 0);
        transShader->SetInt("uMask", 1);
        transShader->SetFloat("uProg", prog);
        transShader->SetFloat("uVague", std::clamp(vague / 255.0f, 0.0f, 1.0f));
        transShader->SetInt("uUseMask", useMask ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, snapshotTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, maskTex ? maskTex : snapshotTex);
        glActiveTexture(GL_TEXTURE0);
        verts.clear();
        Quad(0, 0, (float)kScreenW, (float)kScreenH, 0, 0, 1, 1, 1, 1, 1, 1);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data());
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
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

int RgssUI::MakeWindow(float x, float y, float w, float h) {
    RgssWindowState wnd;
    wnd.id = mNextId++;
    wnd.x = x; wnd.y = y; wnd.width = w; wnd.height = h;
    wnd.seq = mNextSeq++;
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
    // GL-Ressourcen der Bitmap-Registry freigeben (Kontext ist aktiv)
    for (auto& kv : s_bitmaps.items) {
        if (kv.second->textureId) glDeleteTextures(1, &kv.second->textureId);
        kv.second->textureId = 0;
    }
    s_bitmaps.Clear();
    mWindows.clear();
    mNextId = 1;
    s_drawables.Clear();
    s_viewports.Clear();
    s_tables.Clear();
    s_rects.Clear();
    s_colors.Clear();
    s_tones.Clear();
    s_fonts.Clear();
    s_fontDefaults.reset();
    // PAKET 14: Freeze-Snapshot-/Transitionsmasken-Texturen nicht leaken
    // (fallen pro ClearAll an, z. B. Playtest-Stopp)
    if (s_graphics.snapshotTexId) { glDeleteTextures(1, &s_graphics.snapshotTexId); s_graphics.snapshotTexId = 0; }
    if (s_graphics.maskTexId)     { glDeleteTextures(1, &s_graphics.maskTexId);     s_graphics.maskTexId = 0; }
    s_graphics = RgssGraphicsState{};
}

void RgssUI::SetProjectBase(const std::string& basePath) {
    if (s_ProjectBase == basePath) return;
    s_ProjectBase = basePath;
    // Skin-Cache verwerfen (Pfade koennten jetzt anders aufloesen)
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
// Bitmap-Textur (lazy Upload wenn dirty)
// ---------------------------------------------------------------------------
unsigned int RgssUI::BmpTexture(RgssBitmapState* b) {
    if (!b || b->disposed || b->width <= 0 || b->height <= 0) return 0;
    if (b->textureId == 0) {
        glGenTextures(1, &b->textureId);
        b->dirty = true;
    }
    if (b->dirty) {
        glBindTexture(GL_TEXTURE_2D, b->textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, b->width, b->height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, b->pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        b->dirty = false;
    }
    return b->textureId;
}

// ---------------------------------------------------------------------------
// Windowskin (String-Variante, lazy + Cache) - Legacy-Komfort-API
// ---------------------------------------------------------------------------
unsigned int RgssUI::SkinTexture(const std::string& skinName) {
    const std::string key = skinName.empty() ? std::string("__default__") : skinName;
    auto it = mSkinCache.find(key);
    if (it != mSkinCache.end())
        return it->second.texture ? it->second.texture->GetID() : 0;

    std::string name = skinName;
    if (name.empty()) name = Database::Get().System().windowskinName;
    const std::string path = RgssResolveGraphic("Graphics/System/" + name);

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
// 8x8-Font-Atlas (GPU-Variante fuer Fenstertexte)
// ---------------------------------------------------------------------------
unsigned int RgssUI::FontAtlasTexture() {
    if (mFontTextureId) return mFontTextureId;
    FillGlyphBits();
    const int glyphs = 144;
    const int w = 8, h = glyphs * 8;
    std::vector<unsigned char> px((size_t)w * h * 4, 0);
    for (int g = 0; g < glyphs; ++g)
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col) {
                const bool on = (g_glyphBits[g][row] >> (7 - col)) & 1;
                const size_t o = (((size_t)g * 8 + row) * 8 + col) * 4;
                px[o] = px[o + 1] = px[o + 2] = 255;
                px[o + 3] = on ? 255 : 0;
            }

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
// Sprite zeichnen
// ---------------------------------------------------------------------------
void RgssUI::DrawSprite(RgssDrawableState& s, const RgssViewportState* vp) {
    auto* bmp = s_bitmaps.Get(s.bitmapId);
    if (!bmp || bmp->disposed) return;
    if (s.flashHide && s.flashTimer > 0) return; // flash(nil, d): unsichtbar
    const GLuint tex = BmpTexture(bmp);
    if (!tex) return;

    // src_rect: freundliche Abweichung - automatisch volles Bitmap, solange
    // kein src_rect zugewiesen wurde (XP waere 0x0 = unsichtbar)
    float sw = s.srcW, sh = s.srcH, sx = s.srcX, sy = s.srcY;
    if ((sw <= 0 || sh <= 0)) { sx = 0; sy = 0; sw = (float)bmp->width; sh = (float)bmp->height; }
    sw = std::min(sw, (float)bmp->width);
    sh = std::min(sh, (float)bmp->height);

    float dx = s.x - s.ox * s.zoomX;
    float dy = s.y - s.oy * s.zoomY;
    if (vp) { dx -= vp->ox; dy -= vp->oy; }
    const float dw = sw * s.zoomX, dh = sh * s.zoomY;
    if (dw == 0 || dh == 0) return;

    float u0 = sx / bmp->width, v0 = sy / bmp->height;
    float u1 = (sx + sw) / bmp->width, v1 = (sy + sh) / bmp->height;
    if (s.mirror) std::swap(u0, u1);

    // Deckkraft
    float a = std::clamp(s.opacity, 0, 255) / 255.0f;

    // Flash/Color: hoehere Alpha gewinnt (RGSS-Vorgabe)
    float blendCol[4] = { 0, 0, 0, 0 };
    float bestA = -1.0f;
    if (auto* c = RgssColorGet(s.colorId))
        if (c->a > bestA) { blendCol[0] = c->r; blendCol[1] = c->g; blendCol[2] = c->b; blendCol[3] = c->a; bestA = c->a; }
    if (s.flashTimer > 0 && s.flashDuration > 0) {
        if (auto* f = RgssColorGet(s.flashColorId)) {
            const float fa = f->a * ((float)s.flashTimer / (float)s.flashDuration);
            if (fa / 255.0f * 255.0f >= bestA) {
                blendCol[0] = f->r; blendCol[1] = f->g; blendCol[2] = f->b; blendCol[3] = fa;
                bestA = fa;
            }
        }
    }
    float toneArr[4] = { 0, 0, 0, 0 };
    bool hasTone = false;
    if (auto* t = RgssToneGet(s.toneId)) {
        toneArr[0] = t->r; toneArr[1] = t->g; toneArr[2] = t->b; toneArr[3] = t->gray;
        hasTone = true;
    }
    mRenderer->SetState(tex, true, s.blendType, bestA > 0 ? blendCol : nullptr,
                        hasTone ? toneArr : nullptr, 255);

    if (s.angle != 0.0f) {
        float px = s.x, py = s.y;
        if (vp) { px -= vp->ox; py -= vp->oy; }
        mRenderer->QuadRot(dx, dy, dw, dh, px, py, s.angle,
                           u0, v0, u1, v1, 1, 1, 1, a, s.bushDepth);
        mRenderer->Flush();
        return;
    }

    // PAKET 13: XP Sprite#wave — horizontale Sinus-Auslenkung pro Zeile.
    // RGSS-Vorlage: x' = x + wave_amp * sin(2*pi*y/wave_length + wave_phase)
    // (y = Bitmap-Quellzeile, Phase in Grad; Advance laeuft in Sprite#update
    // nativ). Umsetzung: horizontale Streifen (max. 4 px hoch, max. 64).
    // Bewusste Grenzen: rotierte Sprites (angle != 0, s.o.) und der Bush-
    // Weichzeichner bleiben unverzerrt — die Kombination liegt ausserhalb
    // der XP-Default-Skripte. wave_height traegt der XP-Renderer ebenfalls
    // nicht aus (Zustand nur kompatibel gehalten).
    if (std::fabs(s.waveAmp) > 0.01f) {
        const float phaseRad = s.wavePhase * (3.14159265f / 180.0f);
        const float len = s.waveLength > 1.0f ? s.waveLength : 1.0f;
        const int strips = std::max(1, std::min(64, (int)std::ceil(std::fabs(dh) / 4.0f)));
        const float stripH = dh / (float)strips;
        const float dv = (v1 - v0) / (float)strips;
        for (int k = 0; k < strips; ++k) {
            const float srcRow = ((float)k + 0.5f) * (sh / (float)strips);
            const float offX = s.waveAmp *
                std::sin(phaseRad + 2.0f * 3.14159265f * (srcRow / len));
            mRenderer->Quad(dx + offX, dy + k * stripH, dw, stripH,
                            u0, v0 + k * dv, u1, v0 + (k + 1) * dv, 1, 1, 1, a);
        }
        mRenderer->Flush();
        return;
    }

    const float bush = std::clamp(s.bushDepth, 0.0f, dh);
    if (bush > 0.0f && dh > 0.0f) {
        const float topH = dh - bush;
        const float vSplit = v0 + (v1 - v0) * (topH / dh);
        // oberer Teil: volle Deckkraft
        mRenderer->Quad(dx, dy, dw, topH, u0, v0, u1, vSplit, 1, 1, 1, a);
        // unterer Teil (Bush): linear auf 0
        const float col[4][3] = { {1,1,1}, {1,1,1}, {1,1,1}, {1,1,1} };
        mRenderer->Quad4(dx, dy + topH, dw, bush, u0, vSplit, u1, v1, col, a, a, 0, 0);
    } else {
        mRenderer->Quad(dx, dy, dw, dh, u0, v0, u1, v1, 1, 1, 1, a);
    }
    mRenderer->Flush();
}

// ---------------------------------------------------------------------------
// Plane zeichnen (kachelt Bitmap ueber die Flaeche)
// ---------------------------------------------------------------------------
void RgssUI::DrawPlane(RgssDrawableState& p, const RgssViewportState* vp) {
    auto* bmp = s_bitmaps.Get(p.bitmapId);
    if (!bmp || bmp->disposed) return;
    const GLuint tex = BmpTexture(bmp);
    if (!tex) return;

    float areaX = 0, areaY = 0, areaW = (float)kScreenW, areaH = (float)kScreenH;
    if (vp) { areaX = vp->x; areaY = vp->y; areaW = vp->w; areaH = vp->h; }
    if (areaW <= 0 || areaH <= 0) return;
    const float clip[4] = { areaX, areaY, areaW, areaH };
    mRenderer->SetClip(clip);

    float bestA = -1.0f;
    float blendCol[4] = { 0, 0, 0, 0 };
    if (auto* c = RgssColorGet(p.colorId)) {
        blendCol[0] = c->r; blendCol[1] = c->g; blendCol[2] = c->b; blendCol[3] = c->a;
        bestA = c->a;
    }
    float toneArr[4] = { 0, 0, 0, 0 };
    bool hasTone = false;
    if (auto* t = RgssToneGet(p.toneId)) {
        toneArr[0] = t->r; toneArr[1] = t->g; toneArr[2] = t->b; toneArr[3] = t->gray;
        hasTone = true;
    }
    mRenderer->SetState(tex, true, p.blendType, bestA > 0 ? blendCol : nullptr,
                        hasTone ? toneArr : nullptr, 255);

    const float a = std::clamp(p.opacity, 0, 255) / 255.0f;
    const float tileW = bmp->width * p.zoomX, tileH = bmp->height * p.zoomY;
    if (tileW <= 0 || tileH <= 0) { mRenderer->SetClip(nullptr); return; }

    const float baseX = areaX - p.ox - (vp ? vp->ox : 0.0f);
    const float baseY = areaY - p.oy - (vp ? vp->oy : 0.0f);
    const float startX = baseX - std::floor((baseX - areaX) / tileW) * tileW;
    const float startY = baseY - std::floor((baseY - areaY) / tileH) * tileH;
    for (float ty = startY; ty < areaY + areaH; ty += tileH)
        for (float tx = startX; tx < areaX + areaW; tx += tileW)
            mRenderer->Quad(tx, ty, tileW, tileH, 0, 0, 1, 1, 1, 1, 1, a);
    mRenderer->Flush();

    // Viewport-Farbe/Ton als Ueberlagerung (Naeherung)
    if (vp) {
        if (auto* c = RgssColorGet(vp->colorId)) {
            if (c->a > 0) {
                mRenderer->SetState(0, false, 0, nullptr, nullptr, 255);
                mRenderer->Quad(areaX, areaY, areaW, areaH, 0, 0, 0, 0,
                                c->r / 255.0f, c->g / 255.0f, c->b / 255.0f, c->a / 255.0f);
                mRenderer->Flush();
            }
        }
    }
    mRenderer->SetClip(nullptr);
}

// ---------------------------------------------------------------------------
// Paket 6: Prioritaets-Hooks (werden von der Engine einmalig verdrahtet)
static std::function<int(int)> s_prioOfHook;
static std::function<int()>    s_prioMaxHook;
void RgssSetTilePriorityHooks(std::function<int(int)> prioOf,
                              std::function<int()> maxPrio) {
    s_prioOfHook = std::move(prioOf);
    s_prioMaxHook = std::move(maxPrio);
}

static std::function<std::string(int)> s_stepForHook;
static std::function<void(const std::string&)> s_stepPlayHook;
void RgssSetFootstepHooks(std::function<std::string(int)> stepFor,
                          std::function<void(const std::string&)> play) {
    s_stepForHook = std::move(stepFor);
    s_stepPlayHook = std::move(play);
}

// Tilemap zeichnen (3 Ebenen, XP-Autotile-Muster, Prioritaeten, flash_data)
// ---------------------------------------------------------------------------
namespace {
// xp-autotile mustertabelle (48 Muster x 4 Viertel: TL,TR,BL,BR) - Quelle: mkxp
// Koordinaten = 16x16-Quartier im aktuellen 96x128-Frame-Block der Autotiles
const int kAutotilePatterns[48][4][2] = {
    {{32,64},{48,64},{32,80},{48,80}},   {{64,0},{48,64},{32,80},{48,80}},
    {{32,64},{80,0},{32,80},{48,80}},    {{64,0},{80,0},{32,80},{48,80}},
    {{32,64},{48,64},{32,80},{80,16}},   {{64,0},{48,64},{32,80},{80,16}},
    {{32,64},{80,0},{32,80},{80,16}},    {{64,0},{80,0},{32,80},{80,16}},
    {{32,64},{48,64},{64,16},{48,80}},   {{64,0},{48,64},{64,16},{48,80}},
    {{32,64},{80,0},{64,16},{48,80}},    {{64,0},{80,0},{64,16},{48,80}},
    {{32,64},{48,64},{64,16},{80,16}},   {{64,0},{48,64},{64,16},{80,16}},
    {{32,64},{80,0},{64,16},{80,16}},    {{64,0},{80,0},{64,16},{80,16}},
    {{0,64},{16,64},{0,80},{16,80}},     {{0,64},{80,0},{0,80},{16,80}},
    {{0,64},{16,64},{0,80},{80,16}},     {{0,64},{80,0},{0,80},{80,16}},
    {{32,32},{48,32},{32,48},{48,48}},   {{32,32},{48,32},{32,48},{80,16}},
    {{32,32},{48,32},{64,16},{48,48}},   {{32,32},{48,32},{64,16},{80,16}},
    {{64,64},{80,64},{64,80},{80,80}},   {{64,64},{80,64},{64,16},{80,80}},
    {{64,0},{80,64},{64,80},{80,80}},    {{64,0},{80,64},{64,16},{80,80}},
    {{32,96},{48,96},{32,112},{48,112}}, {{64,0},{48,96},{32,112},{48,112}},
    {{32,96},{80,0},{32,112},{48,112}},  {{64,0},{80,0},{32,112},{48,112}},
    {{0,64},{80,64},{0,80},{80,80}},     {{32,32},{48,32},{32,112},{48,112}},
    {{0,32},{16,32},{0,48},{16,48}},     {{0,32},{16,32},{0,48},{80,16}},
    {{64,32},{80,32},{64,48},{80,48}},   {{64,32},{80,32},{64,16},{80,48}},
    {{64,96},{80,96},{64,112},{80,112}}, {{64,0},{80,96},{64,112},{80,112}},
    {{0,96},{16,96},{0,112},{16,112}},   {{0,96},{80,0},{0,112},{16,112}},
    {{0,32},{80,32},{0,48},{80,48}},     {{0,32},{16,32},{0,112},{16,112}},
    {{0,96},{80,96},{0,112},{80,112}},   {{64,32},{80,32},{64,112},{80,112}},
    {{0,32},{80,32},{0,112},{80,112}},   {{0,0},{16,0},{0,16},{16,16}}
};
// Pulsierende Flash-Alpha (32 Frames)
const uint8_t kFlashAlpha[32] = {
    0x3C,0x3C,0x3C,0x3C,0x4B,0x4B,0x4B,0x4B,0x5A,0x5A,0x5A,0x5A,0x69,0x69,0x69,0x69,
    0x78,0x78,0x78,0x78,0x69,0x69,0x69,0x69,0x5A,0x5A,0x5A,0x5A,0x4B,0x4B,0x4B,0x4B
};
} // anonymous namespace

void RgssUI::DrawTilemap(RgssDrawableState& t, const RgssViewportState* vp) {
    auto* mapData = s_tables.Get(t.mapDataId);
    auto* tilesetBmp = s_bitmaps.Get(t.tilesetBmpId);
    RgssTableState* flashData = s_tables.Get(t.flashDataId);
    RgssTableState* priorities = s_tables.Get(t.prioritiesId);
    if (!mapData) return;
    const GLuint tsTex = tilesetBmp ? BmpTexture(tilesetBmp) : 0;

    const bool elevatedPass = false; // (Prioritaeten wirken via z-Split, s. Render)
    (void)elevatedPass;
    const float offX = -(t.ox) - (vp ? vp->ox : 0.0f);
    const float offY = -(t.oy) - (vp ? vp->oy : 0.0f);

    const int xs = mapData->xs, ys = mapData->ys;
    const int layers = std::min(3, mapData->zs);

    // Paket 6 (Terrain-Tag/Schritt-SE): In groben Abstaenden (2,4 s) einen
    // Schritt-Ton abspielen, solange die Tilemap zeichnet. Quelle ist der
    // oberste Tile unter der Bildschirmmitte (Spieler steht dort).
    if (s_stepForHook) {
        static float s_stepTimer = 0.0f;
        s_stepTimer += 1.0f / 60.0f; // Frame-Taktung (~60 fps)
        if (s_stepTimer >= 2.4f) {
            s_stepTimer = 0.0f;
            const int cx = std::clamp((int)std::floor((320.0f - offX) / 32.0f), 0, xs - 1);
            const int cy = std::clamp((int)std::floor((240.0f - offY) / 32.0f), 0, ys - 1);
            int id = 0;
            for (int l = layers - 1; l >= 0 && id <= 0; --l)
                id = mapData->data[((size_t)l * ys + cy) * xs + cx];
            if (id > 0) {
                const std::string nm = s_stepForHook(id);
                if (s_stepPlayHook && !nm.empty()) s_stepPlayHook(nm);
            }
        }
    }
    const auto mapAt = [&](int x, int y, int l) -> int {
        if (x < 0 || y < 0 || x >= xs || y >= ys) return 0;
        return mapData->data[((size_t)l * ys + y) * xs + x];
    };
    int maxPrio = 0;
    if (priorities && (int)priorities->data.size() > 0)
        maxPrio = *std::max_element(priorities->data.begin(), priorities->data.end());
    else if (s_prioMaxHook)
        maxPrio = s_prioMaxHook(); // Paket 6: DB-Fallback
    const int elevatedZ = 32 + std::max(0, maxPrio) * 32 + 32 * std::min(ys, 17);

    // Wir zeichnen in zwei Durchgaengen: pass 0 = Boden (z=0-Yordnung),
    // pass 1 = erhoehte Tiles (ueber den Sprites mit kleinem z).
    // Gesteuert wird das von Render() ueber t.srcW (Missbrauch als Pass-Flag
    // vermeiden - stattdessen t.z = aktueller Pass-Key, siehe Render()).

    GLuint atTex[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int atW[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int atH[7] = { 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 7; ++i) {
        if (auto* at = s_bitmaps.Get(t.autotileBmpId[i])) {
            if (!at->disposed) {
                atTex[i] = BmpTexture(at);
                atW[i] = at->width; atH[i] = at->height;
            }
        }
    }

    for (int l = 0; l < layers; ++l) {
        const int yStart = std::max(0, (int)std::floor((-(offY)) / 32.0f));
        const int yEnd = std::min(ys - 1, yStart + (int)(kScreenH / 32) + 2);
        const int xStart = std::max(0, (int)std::floor((-(offX)) / 32.0f));
        const int xEnd = std::min(xs - 1, xStart + (int)(kScreenW / 32) + 2);
        // laufende Textur pro Batch
        GLuint curBatchTex = 0;
        bool batchOpen = false;
        const auto flushBatch = [&]() { if (batchOpen) { mRenderer->Flush(); batchOpen = false; } };
        const auto setTex = [&](GLuint tx) {
            if (tx != curBatchTex) { flushBatch(); mRenderer->SetState(tx, true, 0, nullptr, nullptr, 255); curBatchTex = tx; batchOpen = true; }
        };
        for (int y = yStart; y <= yEnd; ++y)
            for (int x = xStart; x <= xEnd; ++x) {
                const int id = mapAt(x, y, l);
                if (id <= 0) continue;
                int prio = (priorities && id < (int)priorities->data.size())
                    ? priorities->data[id]
                    : (s_prioOfHook ? s_prioOfHook(id) : 0); // Paket 6: DB-Fallback
                const bool bushed = prio >= 128; // Bit7 = Busch-Flag (nur DB-Pfad)
                prio &= 127;
                const bool elevated = prio > 0;
                if ((t.z == 1) != elevated) continue; // z missbraucht als Pass-Schluessel
                const float dx = x * 32.0f + offX;
                const float dy = y * 32.0f + offY;
                if (id < 384) {
                    // Autotile
                    const int atIdx = std::clamp(id / 48, 0, 6);
                    const GLuint tex = atTex[atIdx];
                    if (!tex) continue;
                    const int pattern = std::clamp(id % 48, 0, 47);
                    const int frames = std::max(1, atW[atIdx] / 96);
                    const int frame = frames > 1 ? (int)((s_graphics.frameCount / 16) % frames) : 0;
                    const float baseX = (float)(frame * 96);
                    // Busch (Paket 6): untere Tile-Haelfte transparent (XP)
                    const bool bThis = bushed;
                    setTex(tex);
                    for (int q = 0; q < 4; ++q) {
                        const float qx = kAutotilePatterns[pattern][q][0];
                        const float qy = kAutotilePatterns[pattern][q][1];
                        const float u0 = (baseX + qx) / atW[atIdx];
                        const float v0 = qy / atH[atIdx];
                        const float u1 = (baseX + qx + 16) / atW[atIdx];
                        const float v1 = (qy + 16) / atH[atIdx];
                        const bool lower = (q / 2) == 1; // untere Subkachel-Zeile
                        const float aHalf = (bThis && lower) ? 0.45f : 1.0f;
                        mRenderer->Quad(dx + (q % 2) * 16.0f, dy + (q / 2) * 16.0f,
                                        16, 16, u0, v0, u1, v1, 1, 1, 1, aHalf);
                    }
                } else {
                    if (!tsTex) continue;
                    setTex(tsTex);
                    const int tId = id - 384;
                    const float su = (float)((tId % 8) * 32);
                    const float sv = (float)((tId / 8) * 32);
                    if ((int)sv >= tilesetBmp->height) continue;
                    if (bushed) {
                        // Busch (Paket 6): weicher Verlauf zur halben
                        // Deckkraft nach unten (XP-Optik "Stehen im Gras")
                        const float white[4][3] = { {1,1,1}, {1,1,1}, {1,1,1}, {1,1,1} };
                        mRenderer->Quad4(dx, dy, 32, 32,
                                         su / tilesetBmp->width, sv / tilesetBmp->height,
                                         (su + 32) / tilesetBmp->width, (sv + 32) / tilesetBmp->height,
                                         white, 1.0f, 1.0f, 0.45f, 0.45f);
                    } else {
                        mRenderer->Quad(dx, dy, 32, 32,
                                        su / tilesetBmp->width, sv / tilesetBmp->height,
                                        (su + 32) / tilesetBmp->width, (sv + 32) / tilesetBmp->height,
                                        1, 1, 1, 1);
                    }
                }
            }
        flushBatch();
    }
    (void)elevatedZ;

    // flash_data (pulsierende Ueberlagerung, z. B. fuer Simulations-Bereiche)
    if (flashData && flashData->xs > 0 && flashData->ys > 0) {
        mRenderer->SetState(0, false, 0, nullptr, nullptr, 255);
        const uint8_t fa = kFlashAlpha[s_graphics.frameCount % 32];
        const int fxr = std::min(flashData->xs, xs), fyr = std::min(flashData->ys, ys);
        for (int y = 0; y < fyr; ++y)
            for (int x = 0; x < fxr; ++x) {
                const int v = flashData->data[(size_t)y * flashData->xs + x];
                if (!v) continue;
                const float r = ((v >> 8) & 0xF) / 15.0f;
                const float g = ((v >> 4) & 0xF) / 15.0f;
                const float b = (v & 0xF) / 15.0f;
                mRenderer->Quad(x * 32.0f + offX, y * 32.0f + offY, 32, 32,
                                0, 0, 0, 0, r, g, b, fa / 255.0f);
            }
        mRenderer->Flush();
    }
}

// ---------------------------------------------------------------------------
// Window zeichnen (volles XP-Windowskin: Hintergrund, Rahmen, Cursor, Pause,
// Contents-Bitmap, Opacities, openness 0..255)
// ---------------------------------------------------------------------------
void RgssUI::DrawWindow(const RgssWindowState& w) {
    if (!mRenderer) return;
    auto* r = mRenderer.get();

    const float op = std::clamp(w.openness, 0.0f, 255.0f) / 255.0f;
    const float hh = w.height * op;
    const float y0 = w.y + (w.height - hh) * 0.5f;
    const float x0 = w.x, ww = w.width;
    if (ww <= 4.0f || hh <= 4.0f) return;

    const RgssViewportState* vp = RgssVpGet(w.viewportId);
    float vpx = 0, vpy = 0;
    if (vp) { vpx = -vp->ox; vpy = -vp->oy; }

    const float winA = std::clamp(w.opacity, 0, 255) / 255.0f;
    const float backA = winA * (std::clamp(w.backOpacity, 0, 255) / 255.0f);
    const float contA = winA * (std::clamp(w.contentsOpacity, 0, 255) / 255.0f);

    // --- Windowskin-Quelle bestimmen (XP: Bitmap aus Ruby; Legacy: String) ---
    GLuint skinTex = 0;
    int skinW = 0, skinH = 0;
    if (auto* sb = s_bitmaps.Get(w.windowskinBmpId)) {
        if (!sb->disposed) {
            skinTex = BmpTexture(sb); skinW = sb->width; skinH = sb->height;
        }
    }
    if (!skinTex) {
        skinTex = SkinTexture(w.windowskin);
        if (auto it = mSkinCache.find(w.windowskin.empty() ? "__default__" : w.windowskin);
            it != mSkinCache.end() && it->second.texture) {
            skinW = it->second.texture->GetWidth();
            skinH = it->second.texture->GetHeight();
        }
    }

    const float bx = x0 + vpx, by = y0 + vpy;

    if (skinTex && skinW > 0 && skinH > 0) {
        const auto uvx = [&](float px) { return px / (float)skinW; };
        const auto uvy = [&](float px) { return px / (float)skinH; };
        // Hintergrund (0,0,128,128): stretch (Standard) oder kacheln
        const float bgW = std::min(128.0f, (float)skinW);
        const float bgH = std::min(128.0f, (float)skinH);
        mRenderer->SetState(skinTex, true, 0, nullptr, nullptr, 255);
        if (w.stretch) {
            mRenderer->Quad(bx, by, ww, hh, uvx(0), uvy(0), uvx(bgW), uvy(bgH),
                            1, 1, 1, backA);
        } else {
            for (float ty = by; ty < by + hh; ty += bgH)
                for (float tx = bx; tx < bx + ww; tx += bgW) {
                    const float tw = std::min(bgW, bx + ww - tx);
                    const float th = std::min(bgH, by + hh - ty);
                    mRenderer->Quad(tx, ty, tw, th,
                                    uvx(0), uvy(0), uvx(tw), uvy(th), 1, 1, 1, backA);
                }
        }
        // Rahmen (128,0,64,64), 16px-Border, 9-Slice
        if (skinW >= 192 && skinH >= 64) {
            const float fx = 128.0f, fy = 0.0f;
            float c = std::min(16.0f, std::min(ww, hh) * 0.5f);
            mRenderer->Quad(bx, by, c, c, uvx(fx), uvy(fy), uvx(fx + c), uvy(fy + c), 1, 1, 1, winA);
            mRenderer->Quad(bx + ww - c, by, c, c, uvx(fx + 64 - c), uvy(fy), uvx(fx + 64), uvy(fy + c), 1, 1, 1, winA);
            mRenderer->Quad(bx, by + hh - c, c, c, uvx(fx), uvy(fy + 64 - c), uvx(fx + c), uvy(fy + 64), 1, 1, 1, winA);
            mRenderer->Quad(bx + ww - c, by + hh - c, c, c, uvx(fx + 64 - c), uvy(fy + 64 - c), uvx(fx + 64), uvy(fy + 64), 1, 1, 1, winA);
            mRenderer->Quad(bx + c, by, ww - 2 * c, c, uvx(fx + c), uvy(fy), uvx(fx + 64 - c), uvy(fy + c), 1, 1, 1, winA);
            mRenderer->Quad(bx + c, by + hh - c, ww - 2 * c, c, uvx(fx + c), uvy(fy + 64 - c), uvx(fx + 64 - c), uvy(fy + 64), 1, 1, 1, winA);
            mRenderer->Quad(bx, by + c, c, hh - 2 * c, uvx(fx), uvy(fy + c), uvx(fx + c), uvy(fy + 64 - c), 1, 1, 1, winA);
            mRenderer->Quad(bx + ww - c, by + c, c, hh - 2 * c, uvx(fx + 64 - c), uvy(fy + c), uvx(fx + 64), uvy(fy + 64 - c), 1, 1, 1, winA);
        }
        mRenderer->Flush();
    } else {
        // Fallback ohne Grafik: dunkles Panel + heller 2px-Rahmen
        mRenderer->SetState(0, false, 0, nullptr, nullptr, 255);
        r->Quad(bx, by, ww, hh, 0, 0, 0, 0, 0.1f, 0.1f, 0.25f, 0.85f * backA);
        r->Quad(bx, by, ww, 2, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, winA);
        r->Quad(bx, by + hh - 2, ww, 2, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, winA);
        r->Quad(bx, by, 2, hh, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, winA);
        r->Quad(bx + ww - 2, by, 2, hh, 0, 0, 0, 0, 0.7f, 0.7f, 0.9f, winA);
        mRenderer->Flush();
    }

    // --- Contents-Bitmap (XP: Fensterinhalt, gescrollt via ox/oy) ---
    const float cx = bx + 16.0f, cy = by + 16.0f;
    const float cw = ww - 32.0f, chh = hh - 32.0f;
    if (cw > 0 && chh > 0) {
        if (auto* cbmp = s_bitmaps.Get(w.contentsBmpId)) {
            if (!cbmp->disposed) {
                const GLuint ctex = BmpTexture(cbmp);
                if (ctex) {
                    const float su = std::clamp(w.contentsOx, 0.0f, (float)cbmp->width);
                    const float sv = std::clamp(w.contentsOy, 0.0f, (float)cbmp->height);
                    const float dw = std::min(cw, (float)cbmp->width - su);
                    const float dh = std::min(chh, (float)cbmp->height - sv);
                    if (dw > 0 && dh > 0) {
                        const float clipRect[4] = { cx, cy, cw, chh };
                        mRenderer->SetClip(clipRect);
                        mRenderer->SetState(ctex, true, 0, nullptr, nullptr, 255);
                        r->Quad(cx, cy, dw, dh,
                                su / cbmp->width, sv / cbmp->height,
                                (su + dw) / cbmp->width, (sv + dh) / cbmp->height,
                                1, 1, 1, contA);
                        mRenderer->Flush();
                        mRenderer->SetClip(nullptr);
                    }
                }
            }
        }
        // --- Cursor (xp: nur wenn aktiv sichtbar blinkend; inaktiv = halb) ---
        const RgssRectState* cr = RgssRectGet(w.cursorRectId);
        if (cr && cr->w > 0 && cr->h > 0 && skinTex && skinW >= 160 && skinH >= 96 && op >= 1.0f) {
            float curA = contA;
            if (w.active) {
                const int t = (int)(s_graphics.frameCount % 32);
                const float pulse = t < 16 ? (1.0f - t / 16.0f) : ((t - 16) / 16.0f);
                curA *= 0.4f + 0.6f * pulse; // 100..255
            } else {
                curA *= 0.5f; // inaktiv: 128
            }
            const auto uvx = [&](float px) { return px / (float)skinW; };
            const auto uvy = [&](float px) { return px / (float)skinH; };
            float ccx = cx + cr->x - 0.0f; // cursor_rect relativ zu (-16,-16) des Fensters
            float ccy = cy + cr->y;
            if (vp) { /* vp-Offset schon in bx/by enthalten */ }
            const float cb = 2.0f; // Cursor-Border
            const float cfx = 128.0f, cfy = 64.0f;
            const float tlx = ccx, tly = ccy, trw = cr->w, trh = cr->h;
            mRenderer->SetClip(nullptr);
            mRenderer->SetState(skinTex, true, 0, nullptr, nullptr, 255);
            r->Quad(tlx, tly, cb, cb, uvx(cfx), uvy(cfy), uvx(cfx + cb), uvy(cfy + cb), 1, 1, 1, curA);
            r->Quad(tlx + trw - cb, tly, cb, cb, uvx(cfx + 32 - cb), uvy(cfy), uvx(cfx + 32), uvy(cfy + cb), 1, 1, 1, curA);
            r->Quad(tlx, tly + trh - cb, cb, cb, uvx(cfx), uvy(cfy + 32 - cb), uvx(cfx + cb), uvy(cfy + 32), 1, 1, 1, curA);
            r->Quad(tlx + trw - cb, tly + trh - cb, cb, cb, uvx(cfx + 32 - cb), uvy(cfy + 32 - cb), uvx(cfx + 32), uvy(cfy + 32), 1, 1, 1, curA);
            r->Quad(tlx + cb, tly, trw - 2 * cb, cb, uvx(cfx + cb), uvy(cfy), uvx(cfx + 32 - cb), uvy(cfy + cb), 1, 1, 1, curA);
            r->Quad(tlx + cb, tly + trh - cb, trw - 2 * cb, cb, uvx(cfx + cb), uvy(cfy + 32 - cb), uvx(cfx + 32 - cb), uvy(cfy + 32), 1, 1, 1, curA);
            r->Quad(tlx, tly + cb, cb, trh - 2 * cb, uvx(cfx), uvy(cfy + cb), uvx(cfx + cb), uvy(cfy + 32 - cb), 1, 1, 1, curA);
            r->Quad(tlx + trw - cb, tly + cb, cb, trh - 2 * cb, uvx(cfx + 32 - cb), uvy(cfy + cb), uvx(cfx + 32), uvy(cfy + 32 - cb), 1, 1, 1, curA);
            r->Quad(tlx + cb, tly + cb, trw - 2 * cb, trh - 2 * cb, uvx(cfx + cb), uvy(cfy + cb), uvx(cfx + 32 - cb), uvy(cfy + 32 - cb), 1, 1, 1, curA * 0.65f);
            mRenderer->Flush();
        }
        // --- Pause-Grafik (4 Frames 16x16 bei (160,64), unten mittig) ---
        if (w.pause && skinTex && skinW >= 192 && skinH >= 96 && op >= 1.0f) {
            const int id4 = (int)((s_graphics.frameCount / 8) % 4);
            const float psx = 160.0f + (id4 % 2) * 16.0f;
            const float psy = 64.0f + (id4 / 2) * 16.0f;
            const auto uvx = [&](float px) { return px / (float)skinW; };
            const auto uvy = [&](float px) { return px / (float)skinH; };
            mRenderer->SetState(skinTex, true, 0, nullptr, nullptr, 255);
            r->Quad(bx + ww * 0.5f - 8.0f, by + hh - 18.0f, 16, 16,
                    uvx(psx), uvy(psy), uvx(psx + 16), uvy(psy + 16), 1, 1, 1, winA);
            mRenderer->Flush();
        }
    }

    // --- Text (Engine-Komfort, 8x8-Font 2x skaliert) ---
    if (!w.text.empty()) {
        const float scale = 2.0f, cwp = 8 * scale, lh = 9 * scale;
        float tx = cx, ty = cy;
        const unsigned int fontTex = FontAtlasTexture();
        mRenderer->SetState(fontTex, true, 0, nullptr, nullptr, 255);
        size_t i = 0;
        const size_t n = w.text.size();
        while (i < n) {
            const char chc = w.text[i];
            if (chc == '\n') { tx = cx; ty += lh; ++i; continue; }
            const int g = RgssGlyphIndex(w.text, i);
            const float v0 = (float)(g * 8) / (float)mFontAtlasHeight;
            const float v1 = (float)((g + 1) * 8) / (float)mFontAtlasHeight;
            r->Quad(tx, ty, cwp, cwp, 0.0f, v0, 1.0f, v1,
                    w.textRed, w.textGreen, w.textBlue, w.textAlpha * contA);
            tx += cwp;
            if (tx + cwp > bx + ww - 16.0f) { tx = cx; ty += lh; }
        }
        mRenderer->Flush();
    }
}

// ---------------------------------------------------------------------------
// Graphics-Overlay (freeze-Snapshot, transition)
// ---------------------------------------------------------------------------
void RgssUI::SnapshotScreen(int screenW, int screenH) {
    auto& g = s_graphics;
    if (screenW <= 0 || screenH <= 0) return;
    std::vector<uint8_t> px((size_t)screenW * screenH * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, screenW, screenH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // vertikal spiegeln (GL-Ursprung unten)
    std::vector<uint8_t> flipped(px.size());
    const size_t rowBytes = (size_t)screenW * 4;
    for (int y = 0; y < screenH; ++y)
        std::memcpy(flipped.data() + (size_t)y * rowBytes,
                    px.data() + (size_t)(screenH - 1 - y) * rowBytes, rowBytes);
    if (!g.snapshotTexId) glGenTextures(1, &g.snapshotTexId);
    glBindTexture(GL_TEXTURE_2D, g.snapshotTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenW, screenH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    g.snapshotW = screenW; g.snapshotH = screenH;
    g.haveSnapshot = true;
}

void RgssGraphicsFreeze() {
    auto& g = s_graphics;
    g.freezeActive = true;       // naechster Render friert den Frame ein
    g.haveSnapshot = false;
}

bool RgssGraphicsHasSnapshot() {  // PAKET 14 (Arbiter-Takt)
    return s_graphics.haveSnapshot;
}

void RgssGraphicsTransition(int durationFrames, const std::string& filename, float vague) {
    auto& g = s_graphics;
    g.freezeActive = false;
    g.transitionActive = s_graphics.haveSnapshot;
    g.transitionDuration = std::max(1, durationFrames);
    g.transitionElapsed = 0;
    g.transitionFile = filename;
    g.transitionVague = vague;
    if (g.maskTexId) { glDeleteTextures(1, &g.maskTexId); g.maskTexId = 0; }
    if (!filename.empty()) {
        const std::string path = RgssResolveGraphic(filename);
        int w = 0, h = 0, n = 0;
        stbi_uc* data = path.empty() ? nullptr : stbi_load(path.c_str(), &w, &h, &n, 1);
        if (data && w > 0 && h > 0) {
            glGenTextures(1, &g.maskTexId);
            glBindTexture(GL_TEXTURE_2D, g.maskTexId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (data) stbi_image_free(data);
    }
}

void RgssUI::DrawGraphicsOverlay() {
    auto& g = s_graphics;
    if (g.transitionActive && g.haveSnapshot && g.snapshotTexId) {
        const float prog = std::clamp(
            (float)g.transitionElapsed / (float)g.transitionDuration, 0.0f, 1.0f);
        mRenderer->DrawTransition(g.snapshotTexId, g.maskTexId, prog,
                                  g.transitionVague, g.maskTexId != 0);
        ++g.transitionElapsed;
        if (g.transitionElapsed > g.transitionDuration) {
            g.transitionActive = false;
            g.haveSnapshot = false;
        }
    } else if (g.freezeActive && g.haveSnapshot && g.snapshotTexId) {
        // eingefrorener Screen bis transition() kommt
        mRenderer->SetState(g.snapshotTexId, true, 0, nullptr, nullptr, 255);
        mRenderer->Quad(0, 0, (float)kScreenW, (float)kScreenH, 0, 0, 1, 1, 1, 1, 1, 1);
        mRenderer->Flush();
    }
}

// ---------------------------------------------------------------------------
// Render-Hauptfunktion
// ---------------------------------------------------------------------------
void RgssUI::Render(int screenWidth, int screenHeight) {
    EnsureRenderer();
    if (!mRenderer) return;

    // Aufraeumen
    mWindows.erase(std::remove_if(mWindows.begin(), mWindows.end(),
        [](const RgssWindowState& w) { return w.disposed; }), mWindows.end());
    for (auto it = s_drawables.items.begin(); it != s_drawables.items.end();) {
        if (it->second->disposed) it = s_drawables.items.erase(it); else ++it;
    }
    for (auto it = s_viewports.items.begin(); it != s_viewports.items.end();) {
        if (it->second->disposed) it = s_viewports.items.erase(it); else ++it;
    }

    // Flash-Ticker (frame-getrieben, kein update()-Zwang)
    for (auto& kv : s_drawables.items)
        if (kv.second->flashTimer > 0) --kv.second->flashTimer;
    for (auto& kv : s_viewports.items)
        if (kv.second->flashTimer > 0) --kv.second->flashTimer;

    // Dirty-Bitmap-Texturen hochladen
    for (auto& kv : s_bitmaps.items)
        if (kv.second->dirty && kv.second->textureId != 0) BmpTexture(kv.second.get());

    // Auch wenn nichts zu zeichnen ist: Transition/Freeze laeuft weiter
    const bool wantOverlay = s_graphics.transitionActive ||
        (s_graphics.freezeActive && s_graphics.haveSnapshot);

    // Render-Items sammeln: Fenster + Drawables (+ Tilemap doppelt: Boden/erhoeht)
    struct Item {
        int kind; // 0 Fenster, 1 Drawable(Boden), 2 Tilemap(erhoeht)
        int refId;
        long long sortKey;
        std::uint64_t seq;
    };
    std::vector<Item> items;
    const auto vpZOf = [](int vpId) -> int {
        if (auto* v = RgssVpGet(vpId)) return v->z;
        return 0;
    };
    for (const auto& w : mWindows) {
        if (!w.visible) continue;
        items.push_back({ 0, w.id,
            ((long long)vpZOf(w.viewportId) << 40) | ((long long)(unsigned)w.z << 8),
            w.seq });
    }
    for (auto& kv : s_drawables.items) {
        auto& d = *kv.second;
        if (!d.visible) continue;
        const RgssViewportState* vp = RgssVpGet(d.viewportId);
        if (d.viewportId != 0 && (!vp || !vp->visible)) continue;
        int zEff = d.z;
        int kind = 1;
        if (d.type == RgssDrawableType::Tilemap) {
            // Zwei Durchgaenge: Boden (z=0) + erhoehte Tiles
            int maxPrio = 0, rows = 17;
            if (auto* prio = s_tables.Get(d.prioritiesId))
                for (auto v : prio->data) maxPrio = std::max(maxPrio, (int)v);
            if (maxPrio <= 0 && s_prioMaxHook)
                maxPrio = s_prioMaxHook(); // Paket 6: DB-Fallback
            if (auto* md = s_tables.Get(d.mapDataId)) rows = std::min(md->ys, 17);
            if (maxPrio > 0) {
                const long long key2 =
                    ((long long)vpZOf(d.viewportId) << 40) |
                    ((long long)(unsigned)(32 + maxPrio * 32 + rows * 32) << 8);
                items.push_back({ 2, kv.first, key2, d.seq });
            }
            zEff = 0;
        }
        items.push_back({ kind, kv.first,
            ((long long)vpZOf(d.viewportId) << 40) | ((long long)(unsigned)zEff << 8),
            d.seq });
    }

    if (!items.empty() || wantOverlay) {
        std::stable_sort(items.begin(), items.end(),
            [](const Item& a, const Item& b) {
                if (a.sortKey != b.sortKey) return a.sortKey < b.sortKey;
                return a.seq < b.seq;
            });

        // RPG-Maker-Optik: der 640x480-Canvas wird mit korrektem 4:3-
        // Seitenverhaeltnis ZENTRIERT abgebildet (vorher: aufs volle
        // Fenster gestreckt — auf 16:9 in die Breite verzerrt).
        int vw = screenWidth, vh = screenHeight;
        if (vw * kScreenH > vh * kScreenW)
            vw = vh * kScreenW / kScreenH;
        else
            vh = vw * kScreenH / kScreenW;
        const int vx = (screenWidth - vw) / 2;
        const int vy = (screenHeight - vh) / 2;
        glViewport(vx, vy, vw, vh);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        mRenderer->Begin(vx, vy, vw, vh);
        int activeClipVp = -999999; // zuletzt gesetzter Clip-Viewport
        for (const auto& it : items) {
            if (it.kind == 0) {
                if (auto* w = GetWindow(it.refId)) {
                    const int vpId = w->viewportId;
                    if (vpId != activeClipVp) {
                        if (const RgssViewportState* vp = RgssVpGet(vpId)) {
                            const float cr[4] = { vp->x, vp->y, vp->w, vp->h };
                            mRenderer->SetClip(cr);
                        } else {
                            mRenderer->SetClip(nullptr);
                        }
                        activeClipVp = vpId;
                    }
                    DrawWindow(*w);
                }
            } else {
                auto* d = s_drawables.Get(it.refId);
                if (!d) continue;
                const int vpId = d->viewportId;
                if (vpId != activeClipVp) {
                    if (const RgssViewportState* vp = RgssVpGet(vpId)) {
                        const float cr[4] = { vp->x, vp->y, vp->w, vp->h };
                        mRenderer->SetClip(cr);
                    } else {
                        mRenderer->SetClip(nullptr);
                    }
                    activeClipVp = vpId;
                }
                switch (d->type) {
                    case RgssDrawableType::Sprite: DrawSprite(*d, RgssVpGet(d->viewportId)); break;
                    case RgssDrawableType::Plane:  DrawPlane(*d, RgssVpGet(d->viewportId)); break;
                    case RgssDrawableType::Tilemap: {
                        const int savedZ = d->z;
                        d->z = (it.kind == 2) ? 1 : 0; // Pass-Schluessel
                        DrawTilemap(*d, RgssVpGet(d->viewportId));
                        d->z = savedZ;
                        break;
                    }
                }
            }
        }
        mRenderer->SetClip(nullptr);
        DrawGraphicsOverlay();
        mRenderer->Flush();

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
    }

    // freeze: Frame NACH dem Zeichnen einfangen
    if (s_graphics.freezeActive && !s_graphics.haveSnapshot)
        SnapshotScreen(screenWidth, screenHeight);

    ++s_graphics.frameCount;
}

} // namespace rpg
