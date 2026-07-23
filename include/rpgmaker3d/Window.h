#pragma once

#include <string>
#include <functional>
#include "Types.h"

struct SDL_Window;

typedef void* SDL_GLContext;

namespace rpg {

class Window {
public:
    Window();
    ~Window();

    bool Create(const std::string& title, int width, int height, bool editorMode = true);

    // "Eingebetteter" Modus: kein eigenes OS-Fenster/GL-Kontext.
    // Fuer Hosts wie den Qt-Editor (QOpenGLWidget stellt den Kontext).
    bool CreateForeign(int width, int height);
    bool IsForeign() const { return mForeign; }
    void SetForeignSize(int width, int height);
    void Destroy();
    void SwapBuffers();
    void PollEvents();
    bool ShouldClose() const { return mShouldClose; }

    SDL_Window* GetNativeWindow() const { return mWindow; }
    SDL_GLContext GetGLContext() const { return mContext; }

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    float GetAspectRatio() const { return static_cast<float>(mWidth) / static_cast<float>(mHeight); }

    void SetTitle(const std::string& title);
    void SetVSync(bool enabled);

    /// Fenster-/Desktop-Vollbild umschalten (XP-Verhalten: Alt+Enter).
    /// Im Foreign-Modus (Qt-Host eingebettetes Widget) bleibt das ein
    /// No-op mit Rueckgabe false; bei SDL-Fehlschlag ebenfalls false.
    bool SetFullscreen(bool fullscreen);
    bool ToggleFullscreen() { return SetFullscreen(!mFullscreen); }
    bool IsFullscreen() const { return mFullscreen; }
    /// Fenster-Groesse zur Laufzeit aendern (Framebuffer folgt beim
    /// naechsten Frame; mWidth/mHeight werden sofort synchronisiert).
    void SetSize(int width, int height);

    bool IsEditorMode() const { return mEditorMode; }

private:
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mContext = nullptr;
    int mWidth = 1280;
    int mHeight = 720;
    bool mShouldClose = false;
    bool mEditorMode = true;
    bool mForeign = false;
    bool mFullscreen = false;
};

} // namespace rpg
