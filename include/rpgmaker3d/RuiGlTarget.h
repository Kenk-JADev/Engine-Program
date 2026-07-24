#pragma once
// RPG Maker 3D - RUI-GL-DrawTarget (PAKET 39)
//
// Eigener OpenGL-3.3-Renderer hinter rui::DrawTarget. Damit braucht die
// gesamte Spielanzeige (RUI-Fenster/Overlays) KEIN Dear ImGui mehr:
//  - Batch aus Dreiecken (pos px, uv, color), ein Shader, Texturen:
//    eingebauter 8x8-Font-Atlas (Font8x8.h, Public Domain) + wechselnde
//    Bild-Texturen (Windowskin, Faces, Pictures).
//  - Clipping ueber glScissor (Stack, verschachtelte Rechtecke geschnitten)
//  - Render-State wird in BeginFrame gesichert und in EndFrame
//    restauriert — der 3D-Pass der Engine bleibt unberuehrt.
//
// Aufruf nur im GL-Kontext-Thread (Player: SDL2; Editor: Qt GL).

#include "rpgmaker3d/Rui.h"
#include <string>
#include <vector>
#include <cstdint>

namespace rpg {

class RuiGlTarget : public rui::DrawTarget {
public:
    RuiGlTarget() = default;
    ~RuiGlTarget();

    /// Shader/VAO/Font-Atlas anlegen. GL-Kontext muss aktuell sein.
    /// Idempotent: true bei Erfolg (danach IsReady()).
    bool Init();
    void Shutdown();
    bool IsReady() const { return mReady; }

    /// Frameklammer: Display-Groesse (px), State sichern/setzen.
    void BeginFrame(float displayW, float displayH);
    /// Reste zeichnen + State restaurieren.
    void EndFrame();

    // --- rui::DrawTarget ---------------------------------------------------
    void FillRect(const rui::Rect& r, const rui::Color4& c, float rounding) override;
    void StrokeRect(const rui::Rect& r, const rui::Color4& c, float t, float rounding) override;
    void Text(float x, float y, const std::string& s, const rui::Color4& c, float scale, int align) override;
    float MeasureText(const std::string& s, float scale) const override;
    float LineHeight(float scale) const override;
    void Image(void* texture, int imgW, int imgH,
               const rui::Rect& src, const rui::Rect& dst, const rui::Color4& tint,
               float rotationDeg) override;
    void Line(float x0, float y0, float x1, float y1, const rui::Color4& c, float thickness) override;
    void FillCircle(float cx, float cy, float radius, const rui::Color4& c) override;
    void ClipPush(const rui::Rect& r) override;
    void ClipPop() override;

private:
    struct Vert { float x, y, u, v, r, g, b, a; };

    void PushVert(float x, float y, float u, float v, const rui::Color4& c);
    void PushTri(float x0, float y0, float x1, float y1, float x2, float y2,
                 const rui::Color4& c);
    void PushQuadUV(const float px[4], const float py[4],
                    float u0, float v0, float u1, float v1, const rui::Color4& c);
    void Flush();
    void UseTexture(unsigned int tex);
    void ApplyClip();
    static int DecodeGlyph(const std::string& s, size_t& i);
    void GlyphUv(int glyph, float& u0, float& v0, float& u1, float& v1) const;
    bool BuildFontAtlas();

    bool mReady = false;
    bool mTried = false;               // Init nur einmal versuchen
    unsigned int mVao = 0;
    unsigned int mVbo = 0;
    unsigned int mShader = 0;
    unsigned int mFontTex = 0;         // 16x16-Glyph-Atlas (128x128)
    unsigned int mWhiteTex = 0;        // 1x1 weiss fuer Primitive
    int mULocDisplay = -1;
    int mULocTex = -1;
    float mDispW = 1280.0f, mDispH = 720.0f;
    unsigned int mCurTex = 0xFFFFFFFFu; // aktuell texturiert
    std::vector<Vert> mVerts;
    std::vector<rui::Rect> mClipStack;

    // Gesicherter GL-State (in BeginFrame gezogen, in EndFrame restauriert)
    struct SavedState {
        int program = 0;
        int vao = 0;
        int tex2d = 0;
        int activeTex = 0;
        bool blend = false;
        bool depth = false;
        bool cull = false;
        bool scissor = false;
        int blendSrcRgb = 0, blendDstRgb = 0; // RGB- und Alpha-Faktoren
        int blendSrcA = 0, blendDstA = 0;     // getrennt (BlendFuncSeparate)
        // PAKET 41: Scissor-Rechteck (Flag allein genuegt nicht, sonst
        // erbt der Host unsere Clip-Box) + ARRAY_BUFFER-Bindung.
        int arrayBuf = 0;
        int scissorBox[4] = {0, 0, 0, 0};
    };
    SavedState mSaved;
    bool mFrameOpen = false;
};

} // namespace rpg
