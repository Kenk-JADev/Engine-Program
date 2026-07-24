#pragma once
// RPG Maker 3D - RUI: Eigenes UI-Framework (PAKET 31)
//
// Ziel der Entscheidung "Eigenes UI-Framework mit Script-Windows":
//  - Retained-Window-Schicht im ENGINE-C++ (Fenster behalten Zustand:
//    Rect, Skin, Oeffnen-Animation, Fokus, z-Ordnung - XP-artig).
//  - Input NATIV (rpg::Input): Tastatur UND Maus (Hover/Click) - der
//    STECKRUEBEN-Schwachpunkt des bisherigen ImGui-Overlays (keine
//    IO-Fuetterung -> tote Klicks) ist damit behoben.
//  - Renderer durch eine schmale DrawTarget-Schnittstelle entkoppelt.
//    Adapter vorerst = ImGui (nur DrawList, kein Input!). Spaeter kann ein
//    eigener GL-Batcher oder die RGSS-Canvas-Schicht folgen, ohne dass
//    die Fenster-Klassen sich aendern.
//  - Script-Windows kommen als Folgepaket: Ruby-Bindings auf genau dieselben
//    Klassen (Rui.Window/Rui.Label/Rui.ListView) - Prinzip wie XP's
//    Window_Base, aber von Anfang an auf das Retained-Modell ausgelegt.
//
// Geometrie: reale Fenster-Pixel (DisplaySize). Eine virtuelle Leinwand
// (640x480-Stretch) ist offen gehalten (Manager::SetVirtualSize).

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "Types.h"

namespace rui {

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    bool Contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct Color4 {
    float r = 1, g = 1, b = 1, a = 1;
    Color4() = default;
    Color4(float rr, float gg, float bb, float aa) : r(rr), g(gg), b(bb), a(aa) {}
    Color4 WithAlpha(float mul) const { return Color4(r, g, b, a * mul); }
};

// ---------------------------------------------------------------------------
// Thema (XP-anmutende Defaults; spaeter aus windowskin.png / Projekt-Config)
// ---------------------------------------------------------------------------
struct Theme {
    Color4 faceColor{0.06f, 0.07f, 0.10f, 0.90f};   // Fensterfuellung
    Color4 faceShadow{0.0f, 0.0f, 0.0f, 0.35f};     // Schattenwurf
    Color4 border{0.92f, 0.92f, 0.98f, 1.0f};       // Rahmenlinie
    Color4 text{0.95f, 0.95f, 0.98f, 1.0f};         // Fliesstext
    Color4 textDisabled{0.55f, 0.55f, 0.60f, 1.0f}; // Hinweise/inaktive Items
    Color4 accent{1.00f, 0.85f, 0.45f, 1.0f};       // Sprechername, Cursor-Text
    Color4 cursorBg{0.18f, 0.22f, 0.34f, 0.85f};    // Listenauswahl-Hintergrund
    Color4 gaugeHp{0.35f, 0.80f, 0.45f, 1.0f};
    Color4 gaugeMp{0.35f, 0.60f, 0.95f, 1.0f};
    float padding = 14.0f;         // Fenster-Innenrand
    float borderWidth = 2.0f;
    float rounding = 6.0f;
    float shadow = 4.0f;
    float rowHeight = 22.0f;       // Listenhoehe pro Eintrag
    float blinkHz = 2.5f;          // Cursor-Blinken
    static const Theme& Get();
};

// ---------------------------------------------------------------------------
// DrawTarget: Sammel-Schnittstelle fuer 2D-Primitive. Implementierung im
// Adapter (PAKET 31: ImGui-DrawList, ausschliesslich Zeichnen - kein IO!).
// ---------------------------------------------------------------------------
class DrawTarget {
public:
    virtual ~DrawTarget() = default;
    virtual void FillRect(const Rect& r, const Color4& c, float rounding = 0.0f) = 0;
    virtual void StrokeRect(const Rect& r, const Color4& c, float t = 1.0f, float rounding = 0.0f) = 0;
    virtual void Text(float x, float y, const std::string& s, const Color4& c, float scale = 1.0f, int align = 0) = 0; // align: 0 links, 1 zentriert, 2 rechts
    virtual float MeasureText(const std::string& s, float scale = 1.0f) const = 0;
    virtual float LineHeight(float scale = 1.0f) const = 0;
    virtual void ClipPush(const Rect& r) = 0;
    virtual void ClipPop() = 0;
};

// ---------------------------------------------------------------------------
// Widgets (retained)
// ---------------------------------------------------------------------------
class Widget {
public:
    virtual ~Widget() = default;
    Rect rect;
    bool visible = true;
    bool enabled = true;
    virtual void Draw(DrawTarget& t) = 0;
    /// Rueckgabe true: Maus-Click wurde konsumiert
    virtual bool OnMouseClick(float mx, float my) { (void)mx; (void)my; return false; }
    virtual void OnMouseMove(float mx, float my) { (void)mx; (void)my; }
};

class Label : public Widget {
public:
    std::string text;
    Color4 color{1, 1, 1, 1};
    float scale = 1.0f;
    int align = 0;
    bool wrap = false;
    void Draw(DrawTarget& t) override;
};

class Gauge : public Widget {
public:
    int current = 100, maximum = 100;
    Color4 color = Color4(0.35f, 0.80f, 0.45f, 1.0f);
    Color4 back  = Color4(0.10f, 0.10f, 0.12f, 1.0f);
    void Draw(DrawTarget& t) override;
};

class Panel : public Widget {
public:
    std::vector<std::unique_ptr<Widget>> children;
    bool skinned = true;  // mit Fensterhaut (Face+Schatten+Rahmen)
    std::function<void()> onClick;  // Click in die Flaeche (kein Kind belegt)
    void Draw(DrawTarget& t) override;
    bool OnMouseClick(float mx, float my) override;
    void OnMouseMove(float mx, float my) override;
};

// Auswahl-Liste mit Cursor (Tastatur nativ ODER Maus-Hover/Click)
class ListView : public Widget {
public:
    struct Item { std::string text; bool enabled = true; };
    std::vector<Item> items;
    int selected = 0;
    int topIndex = 0;             // Scroll-Offset (sichtbarer Fensterausschnitt)
    bool cursorVisible = true;
    std::function<void(int)> onPick;       // Click auf Zeile (Index)
    std::function<void(int)> onHoverItem;  // Hover ueber Zeile (-> Auswahl syncen)
    std::function<void()> onCancel;        // optionale Esc/RMB-Spur
    void Draw(DrawTarget& t) override;
    bool OnMouseClick(float mx, float my) override;
    void OnMouseMove(float mx, float my) override;
    int VisibleRowCount() const;
    int RowAt(float my) const;    // -1 = ausserhalb
    void EnsureSelectedVisible();
};

// ---------------------------------------------------------------------------
// Window: XP-artiger Container (Panel + Oeffnen-Animation + Fokus-Flag).
// openness 0..255: Fenster waechst vertikal auf (wie die Messagebox).
// ---------------------------------------------------------------------------
class Window : public Panel {
public:
    std::string id;
    float openness = 255.0f;   // 0=zu, 255=offen (XP-Massstab)
    float openSpeed = 6.0f;    // pro Sekunde * 255
    bool focus = false;
    void Update(float dt);
    void Open()  { if (openness <= 0.0f) openness = 1.0f; }
    void Close() { /* zu-Animation laeuft in Update */ }
    void SetClosing(bool c) { mClosing = c; }
    bool IsClosing() const { return mClosing; }
    bool IsFullyOpen() const { return openness >= 255.0f; }
    bool IsFullyClosed() const { return openness <= 0.0f; }
    void Draw(DrawTarget& t) override; // skaliert Rect nach openness
private:
    bool mClosing = false;
};

// ---------------------------------------------------------------------------
// Manager: haelt die Fenster (Z-Ordnung = Reihenfolge), routet native
// Eingaben (Maus position + Click von rpg::Input) auf Hover/Click.
// ---------------------------------------------------------------------------
class Manager {
public:
    static Manager& Get();

    void SetTheme(const Theme& t) { mTheme = t; }
    const Theme& GetTheme() const { return mTheme; }

    /// Adapter-Injektion (Engine): zeichnende Schnittstelle pro Frame
    void SetDrawTarget(DrawTarget* t) { mDrawTarget = t; }

    Window& AddWindow(std::unique_ptr<Window> w); // uebernimmt Eigentuemer
    void RemoveWindow(const std::string& id);
    Window* FindWindow(const std::string& id);
    void Clear();

    /// 1x pro Frame: dt-Takt (Oeffnen/Schliessen) + Maus-Routing
    void Update(float dt, float mouseX, float mouseY, bool mousePressed);
    void Draw(); // auf mDrawTarget (no-op ohne Adapter)

    int WindowCount() const { return (int)mWindows.size(); }

private:
    Manager() = default;
    Theme mTheme;
    DrawTarget* mDrawTarget = nullptr;
    std::vector<std::unique_ptr<Window>> mWindows;
};

/// Der aktuelle Frame-Mausstatus (von Update gesetzt, Views lesen ihn)
const rpg::Vec2& GetMouse();
bool GetMousePressed();

} // namespace rui
