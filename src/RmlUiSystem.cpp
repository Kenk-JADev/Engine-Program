#include "rpgmaker3d/RmlUiSystem.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/Database.h"

#include "rmlui_glue/RmlUiRenderGL3.h"

#include <RmlUi/Core.h>
#include <SDL.h>

#include <algorithm>
#include <string>

namespace rpg {

// ===========================================================================
// Embedded demo documents (PoC) - spaeter als .rml/.rcss Dateien im Projekt.
// ===========================================================================
// RmlUi CSS: border-shorthand akzeptiert NUR width+color (kein "solid"/border-style).
static const char* kRc = R"RCSS(
* { box-sizing: border-box; }
body {
    font-family: "DejaVu Sans";
    font-size: 15px;
    color: #e8dcc0;
    background-color: transparent;
}
.rpg-window {
    background-color: #1c1c28f2;
    border-width: 2px;
    border-color: #b98a2f;
    border-radius: 8px;
    padding: 12px;
    width: 300px;
}
.rpg-title {
    font-size: 18px;
    color: #ffd970;
    border-bottom-width: 1px;
    border-bottom-color: #b98a2f;
    padding-bottom: 6px;
    margin-bottom: 10px;
}
.rpg-button {
    background-color: #2f2f44;
    border-width: 1px;
    border-color: #b98a2f;
    border-radius: 4px;
    color: #f0e6cc;
    padding: 6px 12px;
    margin: 3px 0;
    display: block;
    width: 100%;
    text-align: center;
}
.rpg-button:hover {
    background-color: #454568;
    border-color: #ffd970;
}
.rpg-button:active { background-color: #1c1c2c; }
.statbar {
    background-color: #101018;
    border-width: 1px;
    border-color: #5a5a70;
    border-radius: 3px;
    height: 16px;
    margin: 4px 0 8px 0;
    width: 260px;
}
.statbar .fill {
    height: 100%;
    border-radius: 2px;
    min-width: 2px;
}
.hpfill { background-color: #c8413c; }
.mpfill { background-color: #3b6fc8; }
.statlabel { font-size: 12px; color: #b8b0a0; }
.badge {
    background-color: #2f2f44;
    border-width: 1px;
    border-color: #5a5a70;
    border-radius: 4px;
    padding: 2px 8px;
    font-size: 12px;
    color: #9fe0a0;
    display: inline-block;
}
.hint { font-size: 12px; color: #a0a0b0; margin-top: 8px; }
)RCSS";

// data-attr-style statt inline style="width: {{x}}px" (}}px bricht den Rml-Parser).
// message_box: wird aus GameUI::Message gespiegelt (Ruby UI.show_message / Events).
static const char* kGameBody = R"RML(
    <div id="message_box" class="rpg-window" style="position: absolute; left: 50%; bottom: 28px; margin-left: -36%; width: 72%; display: none;">
        <div class="rpg-title">{{message_speaker}}</div>
        <div id="message_text" class="statlabel" style="font-size: 15px; color: #f0e6cc; white-space: pre-wrap;">{{message_text}}</div>
        <div class="hint">E / Enter / Space = weiter</div>
    </div>
    <div id="hud_root" class="rpg-window" style="position: absolute; left: 24px; top: 24px; width: 320px;">
        <div class="rpg-title">Game HUD</div>
        <div>Map: <span class="badge">{{map_name}}</span></div>
        <div style="margin: 6px 0;">FPS: <span class="badge">{{fps}}</span>  Modus: <span class="badge">{{mode}}</span></div>
        <div class="statlabel">HP {{hp}} / {{hp_max}}   Gold {{gold}}</div>
        <div class="statbar"><div class="fill hpfill" data-attr-style="hp_style"></div></div>
        <div class="statlabel">MP {{mp}} / {{mp_max}}</div>
        <div class="statbar"><div class="fill mpfill" data-attr-style="mp_style"></div></div>
        <div id="script_line" class="statlabel" style="margin-top: 8px; color: #ffd970;">{{script_line}}</div>
        <div class="hint">Ruby SceneManager + UI.*  |  F5 Playtest  |  F9 HUD</div>
    </div>
    <!-- Absolute ScreenTexts aus Ruby (bis 6 Stueck) -->
    <div id="st0" class="badge" style="position:absolute; display:none;">{{st0}}</div>
    <div id="st1" class="badge" style="position:absolute; display:none;">{{st1}}</div>
    <div id="st2" class="badge" style="position:absolute; display:none;">{{st2}}</div>
    <div id="st3" class="badge" style="position:absolute; display:none;">{{st3}}</div>
    <div id="st4" class="badge" style="position:absolute; display:none;">{{st4}}</div>
    <div id="st5" class="badge" style="position:absolute; display:none;">{{st5}}</div>
)RML";

static const char* kEditorBody = R"RML(
    <div class="rpg-window" style="position: absolute; right: 24px; top: 24px; width: 300px;">
        <div class="rpg-title">RPG Maker 3D</div>
        <div class="statlabel">{{host_hint}}</div>
        <div style="margin: 8px 0;">FPS: <span class="badge">{{fps}}</span></div>
        <button class="rpg-button" onclick="cmd_play">Playtest (F5)</button>
        <button class="rpg-button" onclick="cmd_save">Speichern (Log)</button>
        <button class="rpg-button" onclick="cmd_quit">Beenden</button>
        <div class="hint">Qt-Editor: mit Qt6 bauen (-DRPGMAKER3D_EDITOR_QT=ON). Ohne Qt: dieser RmlUi-Host.</div>
    </div>
)RML";

// Baut ein komplettes RML-Dokument: RCSS wird inline in <style> injiziert.
static Rml::String BuildDocument(const char* title, const char* body) {
    Rml::String s("<rml><head><title>");
    s += title;
    s += "</title><style>";
    s += kRc;
    s += "</style></head><body data-model=\"engine\">";
    s += body;
    s += "</body></rml>";
    return s;
}

// ===========================================================================
// Rml -> Engine Logger bridge
// ===========================================================================
class RmlUiSystemInterface : public Rml::SystemInterface {
public:
    double GetElapsedTime() override {
        return (double)SDL_GetTicks64() / 1000.0;
    }

    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        switch (type) {
            case Rml::Log::LT_ERROR:
            case Rml::Log::LT_ASSERT: RPG_LOG_ERROR("[RmlUi] " + message); break;
            case Rml::Log::LT_WARNING: RPG_LOG_WARN("[RmlUi] " + message); break;
            default: RPG_LOG_INFO("[RmlUi] " + message); break;
        }
        return true;
    }

    void SetClipboardText(const Rml::String& text) override {
        SDL_SetClipboardText(text.c_str());
    }

    void GetClipboardText(Rml::String& text) override {
        char* ct = SDL_GetClipboardText();
        if (ct) { text = ct; SDL_free(ct); }
    }
};

// ===========================================================================
// Implementation (pimpl)
// ===========================================================================
class RmlUiSystemImpl {
public:
    Engine* engine = nullptr;
    std::unique_ptr<RmlUiSystemInterface> systemInterface;
    std::unique_ptr<RmlUiRenderGL3> renderInterface;
    Rml::Context* editorContext = nullptr;
    Rml::Context* gameContext = nullptr;
    Rml::ElementDocument* editorDoc = nullptr;
    Rml::ElementDocument* gameDoc = nullptr;
    bool visible = true;
    bool initialized = false;

    // Data-model "engine" state (pro Kontext ein Handle)
    Rml::DataModelHandle gameModel{};
    Rml::DataModelHandle editorModel{};
    int fps = 0;
    Rml::String mapName = "Sample Map";
    Rml::String mode = "Editor";
    Rml::String hostHint = "RmlUi Host (ohne Qt-Fenster)";
    Rml::String hpStyle = "width: 169px; height: 100%; background-color: #c8413c;";
    Rml::String mpStyle = "width: 156px; height: 100%; background-color: #3b6fc8;";
    Rml::String messageText = "";
    Rml::String messageSpeaker = "Dialog";
    Rml::String scriptLine = "";
    Rml::String stText[6];
    float stX[6] = {};
    float stY[6] = {};
    bool stOn[6] = {};
    int gold = 0;
    bool messageVisible = false;
    int hp = 65, hpMax = 100;
    int mp = 30, mpMax = 50;
    float fpsTime = 0.0f;
    int fpsFrames = 0;

    void RefreshBarStyles() {
        const int hpW = hpMax > 0 ? (260 * hp / hpMax) : 0;
        const int mpW = mpMax > 0 ? (260 * mp / mpMax) : 0;
        hpStyle = "width: " + std::to_string(hpW) + "px; height: 100%; background-color: #c8413c;";
        mpStyle = "width: " + std::to_string(mpW) + "px; height: 100%; background-color: #3b6fc8;";
    }

    void DirtyAll() {
        auto dirty = [](Rml::DataModelHandle& h) {
            if (!h) return;
            h.DirtyVariable("fps");
            h.DirtyVariable("map_name");
            h.DirtyVariable("mode");
            h.DirtyVariable("host_hint");
            h.DirtyVariable("hp");
            h.DirtyVariable("hp_max");
            h.DirtyVariable("mp");
            h.DirtyVariable("mp_max");
            h.DirtyVariable("hp_style");
            h.DirtyVariable("mp_style");
            h.DirtyVariable("message_text");
            h.DirtyVariable("message_speaker");
            h.DirtyVariable("script_line");
            h.DirtyVariable("gold");
            h.DirtyVariable("st0"); h.DirtyVariable("st1"); h.DirtyVariable("st2");
            h.DirtyVariable("st3"); h.DirtyVariable("st4"); h.DirtyVariable("st5");
        };
        dirty(gameModel);
        dirty(editorModel);
    }

    void ApplyMessageVisibility() {
        if (!gameDoc) return;
        if (auto* box = gameDoc->GetElementById("message_box")) {
            box->SetProperty("display", messageVisible ? "block" : "none");
        }
        // HUD etwas einklappen wenn Dialog offen (optional lesbarer)
        if (auto* hud = gameDoc->GetElementById("hud_root")) {
            hud->SetProperty("opacity", messageVisible ? "0.35" : "1.0");
        }
    }

    bool BindModel(Rml::Context* ctx, Rml::DataModelHandle& outHandle) {
        if (!ctx) return false;
        Rml::DataModelConstructor ctor = ctx->CreateDataModel("engine");
        if (!ctor) return false;
        ctor.Bind("fps", &fps);
        ctor.Bind("map_name", &mapName);
        ctor.Bind("mode", &mode);
        ctor.Bind("host_hint", &hostHint);
        ctor.Bind("hp", &hp);
        ctor.Bind("hp_max", &hpMax);
        ctor.Bind("mp", &mp);
        ctor.Bind("mp_max", &mpMax);
        ctor.Bind("hp_style", &hpStyle);
        ctor.Bind("mp_style", &mpStyle);
        ctor.Bind("message_text", &messageText);
        ctor.Bind("message_speaker", &messageSpeaker);
        ctor.Bind("script_line", &scriptLine);
        ctor.Bind("gold", &gold);
        ctor.Bind("st0", &stText[0]); ctor.Bind("st1", &stText[1]); ctor.Bind("st2", &stText[2]);
        ctor.Bind("st3", &stText[3]); ctor.Bind("st4", &stText[4]); ctor.Bind("st5", &stText[5]);
        RmlUiSystemImpl* impl = this;
        ctor.BindEventCallback("cmd_damage", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            impl->hp = std::max(0, impl->hp - 10);
            impl->RefreshBarStyles();
            impl->DirtyAll();
        });
        ctor.BindEventCallback("cmd_heal", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            impl->hp = std::min(impl->hpMax, impl->hp + 20);
            impl->RefreshBarStyles();
            impl->DirtyAll();
        });
        ctor.BindEventCallback("cmd_new", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            RPG_LOG_INFO("[RmlUi] Editor: Neues Projekt (PoC)");
        });
        ctor.BindEventCallback("cmd_save", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            RPG_LOG_INFO("[RmlUi] Editor: Speichern (PoC)");
        });
        ctor.BindEventCallback("cmd_play", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            if (impl->engine) {
                const bool next = !impl->engine->IsPlaying();
                impl->engine->SetPlaying(next);
                impl->mode = next ? "Playtest" : "Editor";
                impl->DirtyAll();
                RPG_LOG_INFO(std::string("[RmlUi] Playtest ") + (next ? "ON" : "OFF"));
            }
        });
        ctor.BindEventCallback("cmd_quit", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            if (impl->engine) impl->engine->RequestQuit();
        });
        outHandle = ctor.GetModelHandle();
        return true;
    }

    int GetKeyModifiers() const {
        const SDL_Keymod m = SDL_GetModState();
        int r = 0;
        if (m & KMOD_CTRL) r |= Rml::Input::KM_CTRL;
        if (m & KMOD_SHIFT) r |= Rml::Input::KM_SHIFT;
        if (m & KMOD_ALT) r |= Rml::Input::KM_ALT;
        if (m & KMOD_GUI) r |= Rml::Input::KM_META;
        return r;
    }

    static Rml::Input::KeyIdentifier ConvertKey(SDL_Scancode sc) {
        using KI = Rml::Input::KeyIdentifier;
        if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
            return (KI)((int)KI::KI_A + (sc - SDL_SCANCODE_A));
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
            return (KI)((int)KI::KI_1 + (sc - SDL_SCANCODE_1));
        if (sc == SDL_SCANCODE_0) return KI::KI_0;
        if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
            return (KI)((int)KI::KI_F1 + (sc - SDL_SCANCODE_F1));

        switch (sc) {
            case SDL_SCANCODE_SPACE: return KI::KI_SPACE;
            case SDL_SCANCODE_RETURN: return KI::KI_RETURN;
            case SDL_SCANCODE_ESCAPE: return KI::KI_ESCAPE;
            case SDL_SCANCODE_TAB: return KI::KI_TAB;
            case SDL_SCANCODE_BACKSPACE: return KI::KI_BACK;
            case SDL_SCANCODE_DELETE: return KI::KI_DELETE;
            case SDL_SCANCODE_LEFT: return KI::KI_LEFT;
            case SDL_SCANCODE_RIGHT: return KI::KI_RIGHT;
            case SDL_SCANCODE_UP: return KI::KI_UP;
            case SDL_SCANCODE_DOWN: return KI::KI_DOWN;
            case SDL_SCANCODE_HOME: return KI::KI_HOME;
            case SDL_SCANCODE_END: return KI::KI_END;
            case SDL_SCANCODE_PAGEUP: return KI::KI_PRIOR;
            case SDL_SCANCODE_PAGEDOWN: return KI::KI_NEXT;
            case SDL_SCANCODE_LSHIFT: return KI::KI_LSHIFT;
            case SDL_SCANCODE_RSHIFT: return KI::KI_RSHIFT;
            case SDL_SCANCODE_LCTRL: return KI::KI_LCONTROL;
            case SDL_SCANCODE_RCTRL: return KI::KI_RCONTROL;
            case SDL_SCANCODE_LALT: return KI::KI_LMENU;
            case SDL_SCANCODE_RALT: return KI::KI_RMENU;
            default: return KI::KI_UNKNOWN;
        }
    }

    // Forward an event to the currently relevant contexts.
    template <typename F>
    bool ForContexts(F&& fn) {
        // Editor-Kontext zuerst (liegt optisch oben), dann Game-Kontext.
        bool consumed = false;
        if (editorContext && !fn(editorContext)) consumed = true;
        if (gameContext && !consumed && !fn(gameContext)) consumed = true;
        return consumed;
    }
};

// ===========================================================================
// Public facade
// ===========================================================================
RmlUiSystem::RmlUiSystem() = default;
RmlUiSystem::~RmlUiSystem() { Shutdown(); }

bool RmlUiSystem::Initialize(Engine* engine) {
    if (m) return true;
    m = std::make_unique<RmlUiSystemImpl>();
    m->engine = engine;

    Window* window = &engine->GetWindow();
    if (!window) {
        RPG_LOG_ERROR("[RmlUi] Initialize: no window");
        m.reset();
        return false;
    }

    m->systemInterface = std::make_unique<RmlUiSystemInterface>();
    m->renderInterface = std::make_unique<RmlUiRenderGL3>();
    if (!m->renderInterface->Initialize()) {
        RPG_LOG_ERROR("[RmlUi] Initialize: GL backend failed");
        m.reset();
        return false;
    }
    m->renderInterface->SetViewport(window->GetWidth(), window->GetHeight());

    Rml::SetSystemInterface(m->systemInterface.get());
    Rml::SetRenderInterface(m->renderInterface.get());
    if (!Rml::Initialise()) {
        RPG_LOG_ERROR("[RmlUi] Rml::Initialise failed");
        m.reset();
        return false;
    }

    // Font (PoC: Projekt-Font, sonst OS-Fallbacks)
    bool fontOk = Rml::LoadFontFace("assets/fonts/DejaVuSans.ttf", true);
    if (!fontOk) fontOk = Rml::LoadFontFace("C:/Windows/Fonts/segoeui.ttf", true);
    if (!fontOk) fontOk = Rml::LoadFontFace("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", true);
    if (!fontOk) RPG_LOG_WARN("[RmlUi] Kein Font gefunden - Text fehlt im PoC");

    // --- Getrennte Kontexte: editor + game ---
    Rml::Vector2i dims(window->GetWidth(), window->GetHeight());
    m->editorContext = Rml::CreateContext("editor", dims);
    m->gameContext = Rml::CreateContext("game", dims);
    if (!m->editorContext || !m->gameContext) {
        RPG_LOG_ERROR("[RmlUi] CreateContext failed");
        Shutdown();
        return false;
    }

    // Data-Model "engine" in BEIDEN Kontexten (editor-Dokument braucht eigenes Model).
    m->RefreshBarStyles();
#ifdef RPGMAKER3D_EDITOR_QT
    m->hostHint = "Qt-Editor-Host (native Docks + Game View)";
    m->mode = "Qt-Editor";
#else
    m->hostHint = "SDL-Host ohne Qt – RmlUi-Panels aktiv. F5=Playtest, F9=HUD.";
    m->mode = "Editor";
#endif
    if (!m->BindModel(m->gameContext, m->gameModel))
        RPG_LOG_WARN("[RmlUi] DataModel 'engine' (game) fehlgeschlagen");
    if (!m->BindModel(m->editorContext, m->editorModel))
        RPG_LOG_WARN("[RmlUi] DataModel 'engine' (editor) fehlgeschlagen");

    m->gameDoc = m->gameContext->LoadDocumentFromMemory(BuildDocument("RPG Maker 3D - Game HUD", kGameBody));
    if (m->gameDoc) m->gameDoc->Show();
    m->editorDoc = m->editorContext->LoadDocumentFromMemory(BuildDocument("RPG Maker 3D - RmlUi Editor Panel", kEditorBody));
    if (m->editorDoc) m->editorDoc->Show();

    if (!m->gameDoc || !m->editorDoc) {
        RPG_LOG_ERROR("[RmlUi] Dokument-Load fehlgeschlagen (siehe RmlUi-Log oben)");
    } else {
        RPG_LOG_INFO("[RmlUi] initialisiert: 2 Kontexte (editor/game), F9 toggelt Sichtbarkeit");
    }

    // TextInput nur wenn SDL-Video existiert (im Qt-Editor nicht der Fall)
    if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_StartTextInput();
    m->initialized = true;
    return true;
}

void RmlUiSystem::Shutdown() {
    if (!m) return;
    if (m->initialized) {
        Rml::Shutdown(); // gibt Kontexte/Dokumente frei
    }
    m.reset();
}

bool RmlUiSystem::ProcessEvent(const SDL_Event& e) {
    if (!m || !m->initialized || !m->visible) return false;
    const int mods = m->GetKeyModifiers();

    // F9 toggelt die RmlUi-Oberflaeche (unabhaengig von Sichtbarkeit)
    if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_F9) {
        ToggleVisible();
        return true;
    }

    switch (e.type) {
        case SDL_KEYDOWN: {
            const bool keyDown = true;
            return m->ForContexts([&](Rml::Context* c) {
                return c->ProcessKeyDown(RmlUiSystemImpl::ConvertKey(e.key.keysym.scancode), mods) || !keyDown;
            });
        }
        case SDL_KEYUP:
            return m->ForContexts([&](Rml::Context* c) {
                return c->ProcessKeyUp(RmlUiSystemImpl::ConvertKey(e.key.keysym.scancode), mods);
            });
        case SDL_TEXTINPUT:
            return m->ForContexts([&](Rml::Context* c) {
                return c->ProcessTextInput(Rml::String(e.text.text));
            });
        case SDL_MOUSEMOTION:
            return m->ForContexts([&](Rml::Context* c) {
                return c->ProcessMouseMove(e.motion.x, e.motion.y, mods);
            });
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int btn = 0;
            switch (e.button.button) {
                case SDL_BUTTON_LEFT: btn = 0; break;
                case SDL_BUTTON_MIDDLE: btn = 1; break;
                case SDL_BUTTON_RIGHT: btn = 2; break;
                case SDL_BUTTON_X1: btn = 3; break;
                case SDL_BUTTON_X2: btn = 4; break;
            }
            const bool down = (e.type == SDL_MOUSEBUTTONDOWN);
            return m->ForContexts([&](Rml::Context* c) {
                return down ? c->ProcessMouseButtonDown(btn, mods) : c->ProcessMouseButtonUp(btn, mods);
            });
        }
        case SDL_MOUSEWHEEL:
            return m->ForContexts([&](Rml::Context* c) {
                return c->ProcessMouseWheel((float)e.wheel.preciseY, mods);
            });
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                Window* w = &m->engine->GetWindow();
                const int ww = w ? w->GetWidth() : e.window.data1;
                const int hh = w ? w->GetHeight() : e.window.data2;
                m->renderInterface->SetViewport(ww, hh);
                if (m->editorContext) m->editorContext->SetDimensions(Rml::Vector2i(ww, hh));
                if (m->gameContext) m->gameContext->SetDimensions(Rml::Vector2i(ww, hh));
            }
            return false;
        default:
            return false;
    }
}

void RmlUiSystem::SyncFromGameUI() {
    if (!m || !m->initialized) return;

    // Party / Gold
    try {
        auto& party = Game::Get().Party();
        m->gold = party.GetGold();
        if (!party.Members().empty()) {
            m->hp = party.Members()[0].hp;
            m->hpMax = std::max(m->hp, 100);
        }
    } catch (...) {}

    // Dialog aus GameUI (Ruby: UI.show_message / Events)
    auto& msg = GameUI::Get().Message();
    const bool wasVis = m->messageVisible;
    m->messageVisible = msg.IsVisible();
    if (m->messageVisible) {
        m->messageText = msg.GetDisplayedText().empty() ? msg.GetFullText() : msg.GetDisplayedText();
        m->messageSpeaker = msg.GetSpeakerName().empty() ? "Dialog" : msg.GetSpeakerName();
        // position: adjust bottom margin via style
        if (auto* box = m->gameDoc ? m->gameDoc->GetElementById("message_box") : nullptr) {
            if (msg.GetPosition() == 2) { // top
                box->SetProperty("bottom", "auto");
                box->SetProperty("top", "28px");
            } else if (msg.GetPosition() == 1) {
                box->SetProperty("bottom", "40%");
                box->SetProperty("top", "auto");
            } else {
                box->SetProperty("top", "auto");
                box->SetProperty("bottom", "28px");
            }
        }
    } else {
        m->messageText.clear();
        m->messageSpeaker = "Dialog";
    }
    if (wasVis != m->messageVisible) m->ApplyMessageVisibility();

    // ScreenTexts aus Ruby -> RmlUi Overlays + script_line
    const auto& texts = GameUI::Get().GetScreenTexts();
    for (int i = 0; i < 6; ++i) {
        m->stOn[i] = false;
        m->stText[i].clear();
    }
    int n = 0;
    for (const auto& st : texts) {
        if (st.worldSpace) continue; // world texts bleiben optional ungerendert in Rml
        if (n >= 6) break;
        m->stOn[n] = true;
        m->stText[n] = st.text;
        m->stX[n] = st.screenPos.x;
        m->stY[n] = st.screenPos.y;
        ++n;
    }
    if (n > 0) m->scriptLine = m->stText[n - 1];
    else if (!m->messageVisible) m->scriptLine.clear();

    // Positionen auf Elemente anwenden (Prozent der Viewport-Groesse)
    if (m->gameDoc) {
        const char* ids[6] = {"st0","st1","st2","st3","st4","st5"};
        for (int i = 0; i < 6; ++i) {
            Rml::Element* el = m->gameDoc->GetElementById(ids[i]);
            if (!el) continue;
            if (!m->stOn[i]) {
                el->SetProperty("display", "none");
                continue;
            }
            el->SetProperty("display", "block");
            el->SetProperty("left", std::to_string(m->stX[i] * 100.f) + "%");
            el->SetProperty("top", std::to_string(m->stY[i] * 100.f) + "%");
            el->SetProperty("transform", "translate(-50%, -50%)");
            el->SetProperty("pointer-events", "none");
            el->SetProperty("z-index", "20");
        }
    }

    // Map-Name aus Database
    try {
        const int mid = Database::Get().System().startMapId;
        m->mapName = Database::Get().System().gameTitle;
        for (const auto& mi : Database::Get().MapInfos()) {
            if (mi.id == mid) { m->mapName = mi.name; break; }
        }
    } catch (...) {}

    m->RefreshBarStyles();
    m->DirtyAll();
}

void RmlUiSystem::Update(float dt) {
    if (!m || !m->initialized) return;

    // FPS (0.5s Intervall)
    m->fpsTime += dt;
    m->fpsFrames++;
    if (m->fpsTime >= 0.5f) {
        m->fps = (int)((float)m->fpsFrames / m->fpsTime + 0.5f);
        m->fpsFrames = 0;
        m->fpsTime = 0.0f;
        if (m->engine) {
            m->mode = m->engine->IsPlaying() ? "Playtest" : (
#ifdef RPGMAKER3D_EDITOR_QT
                "Qt-Editor"
#else
                "Editor"
#endif
            );
        }
    }

    // Jeden Frame: GameUI (Ruby-Scripts) -> RmlUi HUD
    SyncFromGameUI();

    if (m->visible) {
        if (m->editorContext) m->editorContext->Update();
        if (m->gameContext) m->gameContext->Update();
    }
}

void RmlUiSystem::Render() {
    if (!m || !m->initialized || !m->visible) return;
    m->renderInterface->BeginRender();
    if (m->gameContext) m->gameContext->Render();
    if (m->editorContext) m->editorContext->Render();
    m->renderInterface->EndRender();
}

bool RmlUiSystem::IsVisible() const { return m ? m->visible : false; }
void RmlUiSystem::SetVisible(bool v) { if (m) m->visible = v; }
void RmlUiSystem::ToggleVisible() { if (m) m->visible = !m->visible; }

// ---------------------------------------------------------------------------
// Qt-Editor Input-Bruecke
// ---------------------------------------------------------------------------
namespace {
// Qt::Key Werte (stabil, ohne Qt-Header in dieser TU)
// https://doc.qt.io/qt-6/qt.html#Key-enum
constexpr int QT_Key_Escape = 0x01000000;
constexpr int QT_Key_Tab = 0x01000001;
constexpr int QT_Key_Backspace = 0x01000003;
constexpr int QT_Key_Return = 0x01000004;
constexpr int QT_Key_Enter = 0x01000005;
constexpr int QT_Key_Insert = 0x01000006;
constexpr int QT_Key_Delete = 0x01000007;
constexpr int QT_Key_Pause = 0x01000008;
constexpr int QT_Key_Home = 0x01000010;
constexpr int QT_Key_End = 0x01000011;
constexpr int QT_Key_Left = 0x01000012;
constexpr int QT_Key_Up = 0x01000013;
constexpr int QT_Key_Right = 0x01000014;
constexpr int QT_Key_Down = 0x01000015;
constexpr int QT_Key_PageUp = 0x01000016;
constexpr int QT_Key_PageDown = 0x01000017;
constexpr int QT_Key_Shift = 0x01000020;
constexpr int QT_Key_Control = 0x01000021;
constexpr int QT_Key_Alt = 0x01000023;
constexpr int QT_Key_Meta = 0x01000022;
constexpr int QT_Key_CapsLock = 0x01000024;
constexpr int QT_Key_F1 = 0x01000030;
constexpr int QT_Key_Space = 0x20;
constexpr int QT_Key_0 = 0x30;
constexpr int QT_Key_9 = 0x39;
constexpr int QT_Key_A = 0x41;
constexpr int QT_Key_Z = 0x5a;

Rml::Input::KeyIdentifier ConvertQtKey(int qtKey) {
    using KI = Rml::Input::KeyIdentifier;
    if (qtKey >= QT_Key_A && qtKey <= QT_Key_Z)
        return (KI)((int)KI::KI_A + (qtKey - QT_Key_A));
    if (qtKey >= QT_Key_0 && qtKey <= QT_Key_9)
        return (KI)((int)KI::KI_0 + (qtKey - QT_Key_0));
    if (qtKey >= QT_Key_F1 && qtKey < QT_Key_F1 + 12)
        return (KI)((int)KI::KI_F1 + (qtKey - QT_Key_F1));

    switch (qtKey) {
        case QT_Key_Space: return KI::KI_SPACE;
        case QT_Key_Return:
        case QT_Key_Enter: return KI::KI_RETURN;
        case QT_Key_Escape: return KI::KI_ESCAPE;
        case QT_Key_Tab: return KI::KI_TAB;
        case QT_Key_Backspace: return KI::KI_BACK;
        case QT_Key_Delete: return KI::KI_DELETE;
        case QT_Key_Left: return KI::KI_LEFT;
        case QT_Key_Right: return KI::KI_RIGHT;
        case QT_Key_Up: return KI::KI_UP;
        case QT_Key_Down: return KI::KI_DOWN;
        case QT_Key_Home: return KI::KI_HOME;
        case QT_Key_End: return KI::KI_END;
        case QT_Key_PageUp: return KI::KI_PRIOR;
        case QT_Key_PageDown: return KI::KI_NEXT;
        case QT_Key_Insert: return KI::KI_INSERT;
        case QT_Key_Shift: return KI::KI_LSHIFT;
        case QT_Key_Control: return KI::KI_LCONTROL;
        case QT_Key_Alt: return KI::KI_LMENU;
        case QT_Key_Meta: return KI::KI_LMETA;
        case QT_Key_CapsLock: return KI::KI_CAPITAL;
        case QT_Key_Pause: return KI::KI_PAUSE;
        default: return KI::KI_UNKNOWN;
    }
}

// Qt KeyboardModifiers: Shift=0x02000000, Control=0x04000000, Alt=0x08000000, Meta=0x10000000
int MapQtModifiers(int qtMods) {
    int r = 0;
    if (qtMods & 0x04000000) r |= Rml::Input::KM_CTRL;
    if (qtMods & 0x02000000) r |= Rml::Input::KM_SHIFT;
    if (qtMods & 0x08000000) r |= Rml::Input::KM_ALT;
    if (qtMods & 0x10000000) r |= Rml::Input::KM_META;
    // already-Rml bitmask (small values) pass through
    if (qtMods & Rml::Input::KM_CTRL) r |= Rml::Input::KM_CTRL;
    if (qtMods & Rml::Input::KM_SHIFT) r |= Rml::Input::KM_SHIFT;
    if (qtMods & Rml::Input::KM_ALT) r |= Rml::Input::KM_ALT;
    if (qtMods & Rml::Input::KM_META) r |= Rml::Input::KM_META;
    return r;
}
} // namespace

bool RmlUiSystem::ProcessMouseMove(int x, int y, int modifiers) {
    if (!m || !m->initialized || !m->visible) return false;
    const int mods = MapQtModifiers(modifiers);
    return m->ForContexts([&](Rml::Context* c) {
        return c->ProcessMouseMove(x, y, mods);
    });
}

bool RmlUiSystem::ProcessMouseButton(int button, bool down, int modifiers) {
    if (!m || !m->initialized || !m->visible) return false;
    const int mods = MapQtModifiers(modifiers);
    return m->ForContexts([&](Rml::Context* c) {
        return down ? c->ProcessMouseButtonDown(button, mods)
                    : c->ProcessMouseButtonUp(button, mods);
    });
}

bool RmlUiSystem::ProcessMouseWheel(float deltaY, int modifiers) {
    if (!m || !m->initialized || !m->visible) return false;
    const int mods = MapQtModifiers(modifiers);
    return m->ForContexts([&](Rml::Context* c) {
        return c->ProcessMouseWheel(deltaY, mods);
    });
}

bool RmlUiSystem::ProcessKeyQt(int qtKey, bool down, int modifiers) {
    if (!m || !m->initialized) return false;
    // F9 toggelt auch im Qt-Modus
    if (down && qtKey == (QT_Key_F1 + 8)) { // F9
        ToggleVisible();
        return true;
    }
    if (!m->visible) return false;
    const int mods = MapQtModifiers(modifiers);
    const auto key = ConvertQtKey(qtKey);
    if (key == Rml::Input::KeyIdentifier::KI_UNKNOWN) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return down ? (c->ProcessKeyDown(key, mods) || true)
                    : c->ProcessKeyUp(key, mods);
    });
}

bool RmlUiSystem::ProcessTextInput(const std::string& utf8) {
    if (!m || !m->initialized || !m->visible || utf8.empty()) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return c->ProcessTextInput(Rml::String(utf8));
    });
}

void RmlUiSystem::NotifyViewport(int width, int height) {
    if (!m || !m->initialized || width <= 0 || height <= 0) return;
    if (m->renderInterface) m->renderInterface->SetViewport(width, height);
    if (m->editorContext) m->editorContext->SetDimensions(Rml::Vector2i(width, height));
    if (m->gameContext) m->gameContext->SetDimensions(Rml::Vector2i(width, height));
}

} // namespace rpg
