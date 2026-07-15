#include "rpgmaker3d/RmlUiSystem.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Logger.h"

#include "rmlui_glue/RmlUiRenderGL3.h"

#include <RmlUi/Core.h>
#include <SDL.h>

#include <algorithm>

namespace rpg {

// ===========================================================================
// Embedded demo documents (PoC) - spaeter als .rml/.rcss Dateien im Projekt.
// ===========================================================================
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
    border: 2px solid #b98a2f;
    border-radius: 8px;
    padding: 12px;
    width: 300px;
}
.rpg-title {
    font-size: 18px;
    color: #ffd970;
    border-bottom: 1px solid #b98a2f;
    padding-bottom: 6px;
    margin-bottom: 10px;
}
.rpg-button {
    background-color: #2f2f44;
    border: 1px solid #b98a2f;
    border-radius: 4px;
    color: #f0e6cc;
    padding: 6px 12px;
    margin: 3px 0;
    display: block;
    width: 100%;
    text-align: center;
}
.rpg-button:hover { background-color: #454568; border-color: #ffd970; }
.rpg-button:active { background-color: #1c1c2c; }
.statbar {
    background-color: #101018;
    border: 1px solid #5a5a70;
    border-radius: 3px;
    height: 16px;
    margin: 4px 0 8px 0;
}
.statbar .fill { height: 100%; border-radius: 2px; }
.hpfill { background-color: #c8413c; }
.mpfill { background-color: #3b6fc8; }
.statlabel { font-size: 12px; color: #b8b0a0; }
.badge {
    background-color: #2f2f44;
    border: 1px solid #5a5a70;
    border-radius: 4px;
    padding: 2px 8px;
    font-size: 12px;
    color: #9fe0a0;
    display: inline-block;
}
)RCSS";

static const char* kGameBody = R"RML(
    <div class="rpg-window" style="position: absolute; left: 24px; top: 24px; width: 320px;">
        <div class="rpg-title">RmlUi Game HUD</div>
        <div>Map: <span class="badge">{{map_name}}</span></div>
        <div style="margin: 6px 0;">FPS: <span class="badge">{{fps}}</span> Modus: <span class="badge">{{mode}}</span></div>
        <div class="statlabel">HP {{hp}} / {{hp_max}}</div>
        <div class="statbar"><div class="fill hpfill" style="width: {{hp_width}}px;"></div></div>
        <div class="statlabel">MP {{mp}} / {{mp_max}}</div>
        <div class="statbar"><div class="fill mpfill" style="width: {{mp_width}}px;"></div></div>
        <button class="rpg-button" onclick="cmd_damage">Schaden nehmen</button>
        <button class="rpg-button" onclick="cmd_heal">Heilen</button>
    </div>
)RML";

static const char* kEditorBody = R"RML(
    <div class="rpg-window" style="position: absolute; right: 24px; top: 24px; width: 280px;">
        <div class="rpg-title">RmlUi Editor Panel</div>
        <div class="statlabel">Getrennter UI-Kontext: "editor" (HUD laeuft im "game"-Kontext)</div>
        <div style="margin: 8px 0;">FPS: <span class="badge">{{fps}}</span></div>
        <button class="rpg-button" onclick="cmd_new">Neues Projekt</button>
        <button class="rpg-button" onclick="cmd_save">Speichern</button>
        <button class="rpg-button" onclick="cmd_quit">Beenden</button>
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

    // Data-model "engine" state
    Rml::DataModelHandle model{};
    int fps = 0;
    Rml::String mapName = "Sample Map";
    Rml::String mode = "Editor";
    int hp = 65, hpMax = 100;
    int mp = 30, mpMax = 50;
    float fpsTime = 0.0f;
    int fpsFrames = 0;

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

    // --- Data model "engine" fuer beide Dokumente ---
    {
        Rml::DataModelConstructor ctor = m->gameContext->CreateDataModel("engine");
        if (ctor) {
            ctor.Bind("fps", &m->fps);
            ctor.Bind("map_name", &m->mapName);
            ctor.Bind("mode", &m->mode);
            ctor.Bind("hp", &m->hp);
            ctor.Bind("hp_max", &m->hpMax);
            ctor.Bind("mp", &m->mp);
            ctor.Bind("mp_max", &m->mpMax);
            RmlUiSystemImpl* impl = m.get();
            ctor.BindFunc("hp_width", [impl](Rml::Variant& out) {
                out = Rml::Variant(260 * impl->hp / impl->hpMax);
            });
            ctor.BindFunc("mp_width", [impl](Rml::Variant& out) {
                out = Rml::Variant(260 * impl->mp / impl->mpMax);
            });
            ctor.BindEventCallback("cmd_damage", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                impl->hp = std::max(0, impl->hp - 10);
            });
            ctor.BindEventCallback("cmd_heal", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                impl->hp = std::min(impl->hpMax, impl->hp + 20);
            });
            ctor.BindEventCallback("cmd_new", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                RPG_LOG_INFO("[RmlUi] Editor: Neues Projekt (PoC)");
            });
            ctor.BindEventCallback("cmd_save", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                RPG_LOG_INFO("[RmlUi] Editor: Speichern (PoC)");
            });
            ctor.BindEventCallback("cmd_quit", [impl](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                if (impl->engine) impl->engine->RequestQuit();
            });
            m->model = ctor.GetModelHandle();
        }
    }

    m->gameDoc = m->gameContext->LoadDocumentFromMemory(BuildDocument("RPG Maker 3D - Game HUD", kGameBody));
    if (m->gameDoc) m->gameDoc->Show();
    m->editorDoc = m->editorContext->LoadDocumentFromMemory(BuildDocument("RPG Maker 3D - RmlUi Editor Panel", kEditorBody));
    if (m->editorDoc) m->editorDoc->Show();

    if (!m->gameDoc || !m->editorDoc) {
        RPG_LOG_ERROR("[RmlUi] Dokument-Load fehlgeschlagen (siehe RmlUi-Log oben)");
    } else {
        RPG_LOG_INFO("[RmlUi] PoC initialisiert: 2 Kontexte (editor/game), F9 toggelt Sichtbarkeit");
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

// ---------------------------------------------------------------------------
// SDL-freie Injection (Qt-Editor / externe Hosts)
// ---------------------------------------------------------------------------
bool RmlUiSystem::InjectMouseMove(int x, int y, int rmlKeyMods) {
    if (!m || !m->initialized || !m->visible) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return c->ProcessMouseMove(x, y, rmlKeyMods);
    });
}

bool RmlUiSystem::InjectMouseButton(int rmlButton, bool down, int rmlKeyMods) {
    if (!m || !m->initialized || !m->visible) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return down ? c->ProcessMouseButtonDown(rmlButton, rmlKeyMods)
                    : c->ProcessMouseButtonUp(rmlButton, rmlKeyMods);
    });
}

bool RmlUiSystem::InjectMouseWheel(float delta, int rmlKeyMods) {
    if (!m || !m->initialized || !m->visible) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return c->ProcessMouseWheel(delta, rmlKeyMods);
    });
}

bool RmlUiSystem::InjectKey(int rmlKeyId, bool down, int rmlKeyMods) {
    if (!m || !m->initialized) return false;
    // F9 toggelt die RmlUi-Oberflaeche (gleiches Verhalten wie SDL-Pfad)
    if (down && rmlKeyId == static_cast<int>(Rml::Input::KI_F9)) {
        ToggleVisible();
        return true;
    }
    if (!m->visible) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return down ? c->ProcessKeyDown(static_cast<Rml::Input::KeyIdentifier>(rmlKeyId), rmlKeyMods)
                    : c->ProcessKeyUp(static_cast<Rml::Input::KeyIdentifier>(rmlKeyId), rmlKeyMods);
    });
}

bool RmlUiSystem::InjectText(const char* utf8) {
    if (!m || !m->initialized || !m->visible || !utf8 || !*utf8) return false;
    return m->ForContexts([&](Rml::Context* c) {
        return c->ProcessTextInput(Rml::String(utf8));
    });
}

void RmlUiSystem::SetContextSize(int width, int height) {
    if (!m || !m->initialized || width <= 0 || height <= 0) return;
    m->renderInterface->SetViewport(width, height);
    if (m->editorContext) m->editorContext->SetDimensions(Rml::Vector2i(width, height));
    if (m->gameContext) m->gameContext->SetDimensions(Rml::Vector2i(width, height));
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
    }

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

} // namespace rpg
