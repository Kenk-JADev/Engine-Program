#pragma once
// ============================================================================
// RGSS-Fenstersystem (reine Ruby-UI, XP-artig)
// ----------------------------------------------------------------------------
// Eigenes, RmlUi-UNABHAENGIGES Fenstersystem nur fuer Ruby-Skripte:
//   w = Window.new(64, 80, 320, 180)
//   w.text = "Hallo Welt\nZeile 2"
//   w.windowskin = "001-Blue01"   (Graphics/System/<Name>.png, XP-Format)
//   w.dispose
// Rendering: eigene GL-Overlay-Schicht im logischen 640x480-Raum, gezeichnet
// NACH RmlUi (also ganz oben). Windowskin = XP-Layout (192x128: linker Teil
// 128x128 Rahmen+Hintergrund, rechts Cursor/Pfeile - Cursor folgt spaeter).
// ============================================================================

#include "Types.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rpg {

struct RgssWindowState {
    int id = 0;
    float x = 0.0f, y = 0.0f;
    float width = 0.0f, height = 0.0f;
    int z = 0;
    bool visible = true;
    float openness = 1.0f;      // 0..1, oeffnet vertikal von der Mitte (RGSS)
    std::string windowskin;     // leer = Datenbank-System-Windowskin (dynamisch)
    std::string text;           // contents-Text, Zeilen per '\n'
    float textRed = 1.0f, textGreen = 1.0f, textBlue = 1.0f, textAlpha = 1.0f;
    bool disposed = false;
};

/// Singleton-Verwaltung + eigene GL-Render-Schicht der RGSS-Fenster.
class RgssUI {
public:
    static RgssUI& Get();

    /// Erzeugt ein Fenster, gibt die stabile ID zurueck (>0).
    int CreateWindow(float x, float y, float w, float h);
    void DisposeWindow(int id);
    /// nullptr wenn ID unbekannt/abgeraeumt.
    RgssWindowState* GetWindow(int id);
    /// Alle Ruby-Fenster entfernen (Playtest-Stopp, RGSS-Befehl).
    void ClearAll();
    int WindowCount() const { return (int)mWindows.size(); }

    /// Projekt-Basispfad fuer die Windowskin-Aufloesung setzen (wird von
    /// Engine::LoadCustomConfigForProject gesetzt). Leert den Skin-Cache.
    static void SetProjectBase(const std::string& basePath);
    static const std::string& ProjectBase();

    /// Aus Engine::Render() aufrufen (GL-Kontext aktiv, nach RmlUi).
    /// screenWidth/screenHeight = Framebuffer-Groesse in Pixeln.
    void Render(int screenWidth, int screenHeight);

private:
    RgssUI();
    ~RgssUI(); // out-of-line, weil OverlayRenderer/Texture hier unvollstaendig sind

    struct OverlayRenderer; // GL-Implementierung versteckt in der .cpp
    void EnsureRenderer();
    void CollectWindowVertices(const RgssWindowState& w);
    void DrawWindow(const RgssWindowState& w);
    /// GL-Textur-Id der Windowskin (lazy, mit Cache + Resolver).
    unsigned int SkinTexture(const std::string& skinName);
    /// GL-Textur-Id des 8x8-Bitmap-Font-Atlas (lazy erzeugt).
    unsigned int FontAtlasTexture();

    std::vector<RgssWindowState> mWindows;
    int mNextId = 1;
    std::unique_ptr<OverlayRenderer> mRenderer;

    struct SkinEntry {
        std::shared_ptr<class Texture> texture; // haelt GL-Textur am Leben
    };
    std::unordered_map<std::string, SkinEntry> mSkinCache;
    unsigned int mFontTextureId = 0;
    int mFontAtlasHeight = 0; // in Pixeln (144 Glyphen * 8)
};

} // namespace rpg
