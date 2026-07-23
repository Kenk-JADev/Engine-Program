#pragma once
// ============================================================================
// RGSS-Laufzeit (Ruby Game Scripting System, RPG Maker XP-kompatibel)
// ----------------------------------------------------------------------------
// Vollstaendige, RmlUi-UNABHAENGIGE 2D-Schicht nur fuer Ruby-Skripte.
// Abgedeckt (Spezifikation: RGSS Reference Manual, RPG Maker XP):
//   * Wertklassen:  Rect / Color / Tone / Font (native Registry, echte
//     Referenz-Semantik wie im RGSS: font.color.set(...) wirkt sofort)
//   * Bitmap:       CPU-Pixelpuffer + GL-Textur (fill_rect, gradient_fill_rect,
//                   blt, stretch_blt, draw_text, get/set_pixel, hue_change,
//                   blur, radial_blur, rect, font, text_size, clear, clear_rect)
//   * Table:        1..3-dimensionale int16-Arrays
//   * Drawables:    Sprite (inkl. flash/angle/mirror/bush/blend/tone/color),
//                   Plane (Kachelung, Scroll), Viewport (Clip/Offset/Flash),
//                   Tilemap (3 Ebenen, XP-Autotiles mit echter 48er-Muster-
//                   Tabelle, Prioritaeten, flash_data, Autotile-Animation)
//   * Window:       komplette XP-Fensterklasse (windowskin als Bitmap,
//                   contents-Bitmap, cursor_rect, stretch, opacities, pause,
//                   active-Blinken, openness 0..255)
//   * Graphics:     frame_count/frame_rate, freeze + transition (Crossfade
//                   oder Masken-Transition mit Grafik + vague)
// Rendering: eigene GL-Overlay-Schicht im logischen 640x480-Raum, gezeichnet
// NACH RmlUi (also ganz oben). Windowskin = XP-Layout 192x128:
//   (0,0,128,128) Hintergrund (stretch/tile), (128,0,64,64) Rahmen (16px),
//   (128,64,32,32) Cursor (2px), (160,64,64,32→4x16x16?) -> Pause bei
//   (160,64,32,32) als 4 Frames 16x16, Kampf-Markierungen (128/160,96,32,32).
// ============================================================================

#include "Types.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rpg {

// ---------------------------------------------------------------------------
// Wert-Registries (Rect/Color/Tone/Font) - Ruby-Objekte halten nur eine ID,
// die Werte leben hier => echte RGSS-Referenz-Semantik.
// ---------------------------------------------------------------------------
struct RgssRectState  { float x = 0, y = 0, w = 0, h = 0; };
struct RgssColorState { float r = 0, g = 0, b = 0, a = 0; };        // 0..255
struct RgssToneState  { float r = 0, g = 0, b = 0, gray = 0; };     // -255..255
struct RgssFontState {
    std::string name = "MS PGothic"; // Info-Zweck (Rendering: eingebauter Font)
    int  size = 22;
    bool bold = false;
    bool italic = false;
    bool shadow = false;             // XP Font#shadow: Zustand vollstaendig;
                                     // der eingebaute Text-Renderer zeichnet
                                     // den Schatten Stand heute NICHT mit
                                     // (ehrliche Grenze, s. TODO_XP_PARITY).
    int  colorId = 0;                // -> Color-Registry
};

int  RgssRectCreate(float x, float y, float w, float h);
RgssRectState*  RgssRectGet(int id);
int  RgssColorCreate(float r, float g, float b, float a);
RgssColorState* RgssColorGet(int id);
int  RgssToneCreate(float r, float g, float b, float gray);
RgssToneState*  RgssToneGet(int id);
/// Font mit den aktuellen Font.default_*-Werten.
int  RgssFontCreate();
RgssFontState*  RgssFontGet(int id);
/// Klassenweite Font-Defaults (Font.default_*), anfaenglich XP-Standard.
RgssFontState&  RgssFontDefaults();

// ---------------------------------------------------------------------------
// Table (1-3 dim, int16)
// ---------------------------------------------------------------------------
struct RgssTableState {
    int xs = 1, ys = 1, zs = 1;
    std::vector<int16_t> data;
};
int  RgssTableCreate(int xs, int ys, int zs);
RgssTableState* RgssTableGet(int id);

// ---------------------------------------------------------------------------
// Bitmap
// ---------------------------------------------------------------------------
struct RgssBitmapState {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;     // RGBA, CPU-Seite (get_pixel/set_pixel/...)
    unsigned int textureId = 0;      // GL-Textur (lazy Upload, dirty-getrieben)
    bool dirty = true;
    bool disposed = false;
    int  fontId = 0;                 // Schrift fuer draw_text (Font-Registry)
};

int  RgssBmpCreate(int w, int h);
/// Laedt ein Bild (Aufloesung ueber RgssResolveGraphic). 0 = nicht gefunden.
int  RgssBmpLoad(const std::string& resolvedOrRelative);
/// Tiefer Pixel-Duplikat (Bitmap#clone/dup).
int  RgssBmpClone(int srcId);
RgssBitmapState* RgssBmpGet(int id);
/// Markiert disposed + raeumt die GL-Textur auf (Kontext ist beim Aufruf aktiv).
void RgssBmpDispose(int id);

void RgssBmpFillRect(int id, float x, float y, float w, float h,
                     float r, float g, float b, float a);
void RgssBmpGradientFillRect(int id, float x, float y, float w, float h,
                             float r1, float g1, float b1, float a1,
                             float r2, float g2, float b2, float a2, bool vertical);
void RgssBmpClear(int id);
void RgssBmpClearRect(int id, float x, float y, float w, float h);
void RgssBmpBlt(int dstId, float dx, float dy, int srcId,
                float sx, float sy, float sw, float sh, float opacity);
void RgssBmpStretchBlt(int dstId, float dx, float dy, float dw, float dh,
                       int srcId, float sx, float sy, float sw, float sh, float opacity);
bool RgssBmpGetPixel(int id, int x, int y, float& r, float& g, float& b, float& a);
void RgssBmpSetPixel(int id, int x, int y, float r, float g, float b, float a);
void RgssBmpHueChange(int id, float hueDeg);
void RgssBmpBlur(int id);
void RgssBmpRadialBlur(int id, float angleDeg, int division);
/// draw_text in Pixelpuffer (align 0/1/2), Font aus dem Bitmap-State.
void RgssBmpDrawText(int id, float x, float y, float w, float h,
                     const std::string& utf8, int align);
/// Masse, wenn mit Font des Bitmaps gezeichnet (Text-Width vor 60%-Shrinks).
void RgssBmpTextSize(int id, const std::string& utf8, float& w, float& h);

/// Kanonische Grafik-Aufloesung: relativer Projektpfad (Erweiterung darf
/// fehlen; .png/.jpg/.jpeg/.bmp werden probiert). "" wenn nicht gefunden.
std::string RgssResolveGraphic(const std::string& relPath);

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------
struct RgssViewportState {
    float x = 0, y = 0, w = 0, h = 0;
    int   z = 0;
    float ox = 0, oy = 0;
    bool  visible = true;
    bool  disposed = false;
    int   colorId = 0;             // Mischfarbe (0 = keine)
    int   toneId = 0;              // Farbton (0 = keiner)
    // Flash
    int   flashColorId = 0;        // 0 + flashHide=false => kein Flash
    int   flashDuration = 0, flashTimer = 0;
    bool  flashHide = false;       // flash(nil, n): Viewport kurz wegblenden
};
int  RgssVpCreate(float x, float y, float w, float h);
RgssViewportState* RgssVpGet(int id);
void RgssVpDispose(int id);

// ---------------------------------------------------------------------------
// Drawables: Sprite / Plane / Tilemap
// ---------------------------------------------------------------------------
enum class RgssDrawableType { Sprite = 1, Plane = 2, Tilemap = 3 };

struct RgssDrawableState {
    RgssDrawableType type = RgssDrawableType::Sprite;
    bool  disposed = false;
    bool  visible = true;
    int   z = 0;
    int   viewportId = 0;
    std::uint64_t seq = 0;         // Erstell-Reihenfolge (Gleichstand bei z)

    // Sprite / Plane
    float x = 0, y = 0, ox = 0, oy = 0, zoomX = 1.0f, zoomY = 1.0f;
    float angle = 0;               // Sprite: Grad, gegen Uhrzeiger
    bool  mirror = false;          // Sprite
    float bushDepth = 0;           // Sprite: untere halbtransparente Zone (px)
    // XP Sprite-Wave (Zustand XP-vollstaendig inkl. Phasen-Advance in
    // Sprite#update; der Renderer wendet die Sinusverzerrung Stand heute
    // NICHT an — ehrliche Grenze, s. TODO_XP_PARITY).
    float waveHeight = 0, waveAmp = 0, waveLength = 180.0f, waveSpeed = 360.0f;
    float wavePhase = 0.0f;
    int   opacity = 255;
    int   blendType = 0;           // 0 normal, 1 additiv, 2 subtraktiv
    int   colorId = 0, toneId = 0;
    int   bitmapId = 0;
    float srcX = 0, srcY = 0, srcW = 0, srcH = 0; // src_rect (Sprite)

    // Flash (Sprite)
    int   flashColorId = 0;
    int   flashDuration = 0, flashTimer = 0;
    bool  flashHide = false;

    // Tilemap
    int   tilesetBmpId = 0;
    int   autotileBmpId[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int   mapDataId = 0, flashDataId = 0, prioritiesId = 0;
};

int  RgssDrawableCreate(RgssDrawableType type, int viewportId);
RgssDrawableState* RgssDrawableGet(int id);
void RgssDrawableDispose(int id);

// ---------------------------------------------------------------------------
// Graphics-Modul-Zustand
// ---------------------------------------------------------------------------
struct RgssGraphicsState {
    std::int64_t frameCount = 0;
    int  frameRate = 40;

    // freeze/transition
    bool freezeActive = false;     // freeze aufgerufen -> naechster Frame wird eingefroren
    bool haveSnapshot = false;
    unsigned int snapshotTexId = 0;
    int  snapshotW = 0, snapshotH = 0;

    bool transitionActive = false;
    int  transitionDuration = 8;
    int  transitionElapsed = 0;
    std::string transitionFile;    // leer = Standard-Crossfade
    float transitionVague = 40.0f;
    unsigned int maskTexId = 0;    // Transitionsgrafik (Graustufen-Maske)
};
RgssGraphicsState& RgssGraphics();
void RgssGraphicsFreeze();
void RgssGraphicsTransition(int durationFrames, const std::string& filename, float vague);

// ---------------------------------------------------------------------------
// Window (komplette XP-Fensterklasse)
// ---------------------------------------------------------------------------
struct RgssWindowState {
    int id = 0;
    float x = 0.0f, y = 0.0f;
    float width = 0.0f, height = 0.0f;
    int z = 0;
    bool visible = true;
    float openness = 255.0f;       // 0..255 (XP), oeffnet von der Mitte
    std::string windowskin;        // LEGACY-String (Name), siehe windowskinBmpId
    int  windowskinBmpId = 0;      // XP: windowskin ist ein Bitmap (hat Vorrang)
    std::string text;              // Engine-Komfort: direkter Fenstertext
    float textRed = 1.0f, textGreen = 1.0f, textBlue = 1.0f, textAlpha = 1.0f;
    bool disposed = false;
    // --- XP-Vollset ---
    int  viewportId = 0;
    int  contentsBmpId = 0;        // lazy: Bitmap.new(w-32, h-32) bei Zugriff
    int  cursorRectId = 0;         // Rect-Registry
    bool active = true;
    bool pause = false;
    bool stretch = true;
    int  opacity = 255;            // Rahmen+Hintergrund+Contents gesamt
    int  backOpacity = 255;        // nur Hintergrund
    int  contentsOpacity = 255;    // nur Contents (+Cursor)
    float contentsOx = 0, contentsOy = 0; // Ruby ox/oy (Scrollen des Inhalts)
    std::uint64_t seq = 0;
};

/// Paket 6 (XP-Prioritaet): Fallback-Hooks, wenn ein Tilemap-Drawable KEINE
/// priorities-Table traegt. XP holt die Prioritaeten dann aus den Tileset-
/// Daten ($data_tilesets) - die Engine verdrahtet hier die Paket-1-Tables
/// (TilesetData) der aktiven Karte. prioOf bekommt die RGSS-Tile-ID
/// (0..383 = Autotile-Slots, 384+ = Standard-Tiles); maxPrio liefert den
/// Maximalwert fuer die z-Sortierung (0 = kein erhoehter Durchgang).
void RgssSetTilePriorityHooks(std::function<int(int)> prioOf,
                              std::function<int()> maxPrio);

/// Paket 6 (Terrain-Tag/Schritt-SE): Footstep-Hooks, von der Engine
/// verdrahtet. stepFor(rgssTileId) -> "grass"/"stone"/"water" oder ""
/// (kein Terrain-Tag bzw. generisches Gerdausch); play(name) spielt den
/// Soundeffekt ab. Tag-Semantik (von der Engine implementiert):
/// 1 = Gras, 2 = Stein, 3 = Wasser, Tag 0 = kein Ton.
void RgssSetFootstepHooks(std::function<std::string(int)> stepFor,
                          std::function<void(const std::string&)> play);

/// Singleton-Verwaltung + eigene GL-Render-Schicht der RGSS-Objekte.
class RgssUI {
public:
    static RgssUI& Get();

    /// Erzeugt ein Fenster, gibt die stabile ID zurueck (>0).
    /// WICHTIG: Nicht "CreateWindow" nennen -- windows.h definiert dafuer
    /// ein Makro (CreateWindowW), das Windows-Builds zerbricht.
    int MakeWindow(float x, float y, float w, float h);
    void DisposeWindow(int id);
    /// nullptr wenn ID unbekannt/abgeraeumt.
    RgssWindowState* GetWindow(int id);
    /// Alles abraeumen (Playtest-Stopp / RGSS.clear_windows):
    /// Fenster, Drawables, Viewports, Bitmaps, Tables, Wert-Registries,
    /// Graphics-Zustand und die GL-Texturen.
    void ClearAll();
    int WindowCount() const { return (int)mWindows.size(); }

    /// Projekt-Basispfad fuer die Grafik-Aufloesung setzen (wird von
    /// Engine::LoadCustomConfigForProject gesetzt). Leert die Caches.
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
    void DrawWindow(const RgssWindowState& w);
    void DrawSprite(RgssDrawableState& s, const RgssViewportState* vp);
    void DrawPlane(RgssDrawableState& p, const RgssViewportState* vp);
    void DrawTilemap(RgssDrawableState& t, const RgssViewportState* vp);
    void DrawGraphicsOverlay();
    /// Bitmap-Textur (lazy Upload wenn dirty), 0 wenn ungueltig.
    unsigned int BmpTexture(RgssBitmapState* b);
    /// GL-Textur-Id der String-Windowskin (lazy, Cache + Resolver).
    unsigned int SkinTexture(const std::string& skinName);
    /// GL-Textur-Id des 8x8-Bitmap-Font-Atlas (lazy erzeugt).
    unsigned int FontAtlasTexture();
    void SnapshotScreen(int screenW, int screenH);

    std::vector<RgssWindowState> mWindows;
    int mNextId = 1;
    std::uint64_t mNextSeq = 1;
    std::unique_ptr<OverlayRenderer> mRenderer;

    struct SkinEntry {
        std::shared_ptr<class Texture> texture; // haelt GL-Textur am Leben
    };
    std::unordered_map<std::string, SkinEntry> mSkinCache;
    unsigned int mFontTextureId = 0;
    int mFontAtlasHeight = 0; // in Pixeln (144 Glyphen * 8)
};

} // namespace rpg
