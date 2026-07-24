// RPG Maker 3D - RUI (PAKET 31) - siehe include/rpgmaker3d/Rui.h
#include "rpgmaker3d/Rui.h"
#include <algorithm>
#include <cmath>

#ifdef RPGMAKER3D_ENABLE_IMGUI
#include <imgui.h>
#endif

namespace rui {

static rpg::Vec2 gMouse{0.0f, 0.0f};
static bool gMousePressed = false;
const rpg::Vec2& GetMouse() { return gMouse; }
bool GetMousePressed() { return gMousePressed; }

// Zeitbasis fuer Cursor-/Blink-Animationen (von Manager::Update getaktet)
static float gBlinkTime = 0.0f;
float GetBlinkTime() { return gBlinkTime; }

const Theme& Theme::Get() {
    static Theme s;
    return s;
}

// ---------------------------------------------------------------------------
// DrawTarget-Adapter: ImGui-Eigenstaendige DrawList (NUR Zeichnen -
// ImGui erhaelt von uns weder Tasten- noch Maus-Events; alle Eingaben
// laufen nativ ueber rpg::Input -> Rui::Manager::Update).
// ---------------------------------------------------------------------------
#ifdef RPGMAKER3D_ENABLE_IMGUI
namespace {
static ImU32 ToIm(const Color4& c) {
    return IM_COL32((int)(std::clamp(c.r, 0.f, 1.f) * 255),
                    (int)(std::clamp(c.g, 0.f, 1.f) * 255),
                    (int)(std::clamp(c.b, 0.f, 1.f) * 255),
                    (int)(std::clamp(c.a, 0.f, 1.f) * 255));
}
class ImDrawTarget : public DrawTarget {
public:
    explicit ImDrawTarget(ImDrawList* dl) : mDl(dl) {}
    void FillRect(const Rect& r, const Color4& c, float rounding) override {
        mDl->AddRectFilled(ImVec2(r.x, r.y), ImVec2(r.x + r.w, r.y + r.h),
                           ToIm(c), rounding);
    }
    void StrokeRect(const Rect& r, const Color4& c, float t, float rounding) override {
        mDl->AddRect(ImVec2(r.x, r.y), ImVec2(r.x + r.w, r.y + r.h),
                     ToIm(c), rounding, 0, t);
    }
    void Text(float x, float y, const std::string& s, const Color4& c, float scale, int align) override {
        const float w = MeasureText(s, scale);
        float px = x;
        if (align == 1) px = x - w * 0.5f;
        else if (align == 2) px = x - w;
        mDl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
                     ImVec2(px, y), ToIm(c), s.c_str());
    }
    float MeasureText(const std::string& s, float scale) const override {
        const ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(
            ImGui::GetFontSize() * scale, FLT_MAX, 0.0f, s.c_str());
        return sz.x;
    }
    float LineHeight(float scale) const override { return ImGui::GetFontSize() * scale; }
    void ClipPush(const Rect& r) override {
        mDl->PushClipRect(ImVec2(r.x, r.y), ImVec2(r.x + r.w, r.y + r.h), true);
    }
    void ClipPop() override { mDl->PopClipRect(); }
private:
    ImDrawList* mDl;
};
} // namespace
#endif

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------
void Label::Draw(DrawTarget& t) {
    if (!visible) return;
    const Color4 c = enabled ? color : Theme::Get().textDisabled;
    if (!wrap || t.MeasureText(text, scale) <= rect.w) {
        const float tx = (align == 0) ? rect.x
                         : (align == 1) ? rect.x + rect.w * 0.5f
                                        : rect.x + rect.w;
        t.Text(tx, rect.y, text, c, scale, align);
        return;
    }
    // Wrap: wortweiser Umbruch auf rect.w (ganze UTF-8 nicht halbieren -
    // der Umbruch trennt ausschliesslich an Leerzeichen)
    std::string line;
    size_t pos = 0;
    float y = rect.y;
    const float lineH = t.LineHeight(scale);
    while (pos < text.size() && y + lineH <= rect.y + rect.h) {
        size_t nl = text.find(' ', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string word = text.substr(pos, nl - pos);
        if (!line.empty() && t.MeasureText(line + " " + word, scale) > rect.w) {
            t.Text(rect.x, y, line, c, scale, 0);
            y += lineH;
            line = word;
        } else {
            if (!line.empty()) line += " ";
            line += word;
        }
        pos = nl + (nl < text.size() ? 1 : 0);
    }
    if (!line.empty() && y + lineH <= rect.y + rect.h)
        t.Text(rect.x, y, line, c, scale, 0);
}

void Gauge::Draw(DrawTarget& t) {
    if (!visible) return;
    const float frac = maximum > 0
        ? std::clamp((float)current / (float)maximum, 0.0f, 1.0f) : 0.0f;
    t.FillRect(rect, back, 2.0f);
    t.FillRect(Rect{rect.x, rect.y, rect.w * frac, rect.h}, color, 2.0f);
}

void Panel::Draw(DrawTarget& t) {
    if (!visible) return;
    const Theme& th = Theme::Get();
    if (skinned) {
        Rect shadow{rect.x + th.shadow, rect.y + th.shadow, rect.w, rect.h};
        t.FillRect(shadow, th.faceShadow, th.rounding);
        t.FillRect(rect, th.faceColor, th.rounding);
        t.StrokeRect(rect, th.border, th.borderWidth, th.rounding);
    }
    for (auto& c : children) c->Draw(t);
}

bool Panel::OnMouseClick(float mx, float my) {
    if (!visible || !enabled) return false;
    if (!rect.Contains(mx, my)) return false; // Klick ausserhalb: durchreichen
    for (auto& c : children) {
        if (c->OnMouseClick(mx, my)) return true;
    }
    if (onClick) onClick();              // Flaechen-Click (z.B. Text vorspulen)
    return true;                         // Fenster haelt Klicks im eigenen Rechteck
}

void Panel::OnMouseMove(float mx, float my) {
    if (!visible) return;
    for (auto& c : children) c->OnMouseMove(mx, my);
}

int ListView::VisibleRowCount() const {
    const Theme& th = Theme::Get();
    return std::max(1, (int)(rect.h / th.rowHeight));
}

int ListView::RowAt(float my) const {
    if (!rect.Contains(GetMouse().x, my)) return -1;
    const Theme& th = Theme::Get();
    const int row = (int)((my - rect.y) / th.rowHeight);
    const int idx = topIndex + row;
    return (idx >= 0 && idx < (int)items.size()) ? idx : -1;
}

void ListView::EnsureSelectedVisible() {
    const int vis = VisibleRowCount();
    if (selected < topIndex) topIndex = selected;
    if (selected >= topIndex + vis) topIndex = std::max(0, selected - vis + 1);
    topIndex = std::clamp(topIndex, 0, std::max(0, (int)items.size() - vis));
}

void ListView::Draw(DrawTarget& t) {
    if (!visible) return;
    const Theme& th = Theme::Get();
    EnsureSelectedVisible();
    const int vis = VisibleRowCount();
    t.ClipPush(rect);
    const float blink = cursorVisible
        ? (0.55f + 0.45f * (float)std::sin(GetBlinkTime() * 6.2831853f)) : 0.0f;
    for (int i = 0; i < vis; ++i) {
        const int idx = topIndex + i;
        if (idx >= (int)items.size()) break;
        const auto& it = items[idx];
        Rect row{rect.x, rect.y + i * th.rowHeight, rect.w, th.rowHeight};
        if (idx == selected && cursorVisible) {
            t.FillRect(row, th.cursorBg.WithAlpha(0.65f + 0.35f * blink), 3.0f);
        }
        const bool hovered = rect.Contains(GetMouse().x, GetMouse().y) &&
                             RowAt(GetMouse().y) == idx;
        Color4 c = it.enabled ? (hovered ? th.text : th.text.WithAlpha(0.92f))
                              : th.textDisabled;
        if (!it.enabled) c = th.textDisabled;
        t.Text(row.x + 6.0f, row.y + 2.0f, it.text, c, 1.0f, 0);
    }
    t.ClipPop();
}

bool ListView::OnMouseClick(float mx, float my) {
    if (!visible || !enabled || !rect.Contains(mx, my)) return false;
    const int idx = RowAt(my);
    if (idx < 0) return true; // im rect aber auf keiner Zeile: click verschluckt
    selected = idx;
    if (onPick) onPick(idx);
    return true;
}

void ListView::OnMouseMove(float mx, float my) {
    if (!visible || !enabled) return;
    const int idx = RowAt(my);
    if (idx >= 0 && items[idx].enabled) {
        const bool changed = selected != idx;
        selected = idx;
        if (changed && onHoverItem) onHoverItem(idx);
    }
}

void Window::Update(float dt) {
    const float step = openSpeed * 255.0f * dt;
    if (mClosing) {
        openness = std::max(0.0f, openness - step);
    } else if (openness < 255.0f) {
        openness = std::min(255.0f, openness + step);
    }
}

void Window::Draw(DrawTarget& t) {
    if (!visible || openness <= 0.0f) return;
    if (openness >= 255.0f) { Panel::Draw(t); return; }
    // XP-Oeffnen: Haut waechst vertikal aus der Mittelachse, Inhalte
    // (children) erscheinen erst bei vollem Oeffnen.
    const float f = openness / 255.0f;
    const float h = rect.h * f;
    Rect grown{rect.x, rect.y + (rect.h - h) * 0.5f, rect.w, h};
    Panel skin;
    skin.rect = grown;
    skin.skinned = skinned;
    skin.Draw(t);
}

// ---------------------------------------------------------------------------
// Manager
// ---------------------------------------------------------------------------
Manager& Manager::Get() {
    static Manager s;
    return s;
}

Window& Manager::AddWindow(std::unique_ptr<Window> w) {
    mWindows.push_back(std::move(w));
    return *mWindows.back();
}

void Manager::RemoveWindow(const std::string& id) {
    mWindows.erase(std::remove_if(mWindows.begin(), mWindows.end(),
        [&](const std::unique_ptr<Window>& w) { return w->id == id; }),
        mWindows.end());
}

Window* Manager::FindWindow(const std::string& id) {
    for (auto& w : mWindows) if (w->id == id) return w.get();
    return nullptr;
}

void Manager::Clear() { mWindows.clear(); }

void Manager::Update(float dt, float mouseX, float mouseY, bool mousePressed) {
    gBlinkTime += dt * GetTheme().blinkHz;   // blinkHz*2pi in Draw
    gMouse = rpg::Vec2(mouseX, mouseY);
    gMousePressed = mousePressed;
    for (auto& w : mWindows) w->Update(dt);
    // Maus: Hover von oben nach unten (z-Ordnung), Click konsumiert das
    // oberste belegte Widget.
    for (auto it = mWindows.rbegin(); it != mWindows.rend(); ++it) {
        (*it)->OnMouseMove(mouseX, mouseY);
        if (mousePressed) {
            if ((*it)->OnMouseClick(mouseX, mouseY)) break;
        }
    }
    // Voll geschlossene Fenster entfernen (nach Close-Animation)
    mWindows.erase(std::remove_if(mWindows.begin(), mWindows.end(),
        [](const std::unique_ptr<Window>& w) {
            return w->IsClosing() && w->IsFullyClosed();
        }), mWindows.end());
}

void Manager::Draw() {
#ifdef RPGMAKER3D_ENABLE_IMGUI
    if (!mDrawTarget) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##RuiCanvas", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
        ImDrawTarget dt(ImGui::GetWindowDrawList());
        for (auto& w : mWindows) w->Draw(dt);
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }
#endif
    if (mDrawTarget)
        for (auto& w : mWindows) w->Draw(*mDrawTarget);
}

} // namespace rui
