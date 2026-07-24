#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Config.h"
#include "rpgmaker3d/Logger.h"
#include <iostream>

#if defined(_WIN32)
#include <SDL.h>
#else
#include <SDL.h>
#endif

#include <glad/gl.h>

namespace rpg {

Window::Window() = default;
Window::~Window() { Destroy(); }

bool Window::Create(const std::string& title, int width, int height, bool editorMode) {
    mWidth = width;
    mHeight = height;
    mEditorMode = editorMode;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        RPG_LOG_ERROR(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, EngineConfig::OPENGL_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, EngineConfig::OPENGL_MINOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

#ifdef _WIN32
    // Windows-spezifisch: High DPI aktivieren (wird auch über Platform gesetzt)
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (editorMode) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

#ifdef _WIN32
    // Auf Windows: Beim Editor groß starten, beim Player zentriert
    if (!editorMode) {
        flags &= ~SDL_WINDOW_MAXIMIZED;
    }
#endif

    mWindow = SDL_CreateWindow(title.c_str(),
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               mWidth, mHeight, flags);
    if (!mWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        RPG_LOG_ERROR(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    mContext = SDL_GL_CreateContext(mWindow);
    if (!mContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        RPG_LOG_ERROR(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
        return false;
    }

    // VSync initial deaktivieren für schnelleres Laden, dann aktivieren
    SDL_GL_SetSwapInterval(0);

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (version == 0) {
        std::cerr << "gladLoadGL failed" << std::endl;
        RPG_LOG_ERROR("gladLoadGL failed - OpenGL 3.3 not supported?");
        // Versuche zu prüfen welche Version wir haben
        const char* glVersion = (const char*)glGetString(GL_VERSION);
        if (glVersion) RPG_LOG_INFO(std::string("GL_VERSION: ") + glVersion);
        return false;
    }

    // OpenGL Info loggen
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* ver = (const char*)glGetString(GL_VERSION);
    RPG_LOG_INFO(std::string("OpenGL Vendor: ") + (vendor ? vendor : "Unknown"));
    RPG_LOG_INFO(std::string("OpenGL Renderer: ") + (renderer ? renderer : "Unknown"));
    RPG_LOG_INFO(std::string("OpenGL Version: ") + (ver ? ver : "Unknown"));
    RPG_LOG_INFO("GLAD GL Version: " + std::to_string(GLAD_VERSION_MAJOR(version)) + "." + std::to_string(GLAD_VERSION_MINOR(version)));

    SetVSync(true);

    // Viewport und State
    glViewport(0, 0, mWidth, mHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

#ifdef _WIN32
    // Icon setzen falls vorhanden (Windows)
    std::string iconPath = "assets/icon.bmp";
    SDL_Surface* icon = SDL_LoadBMP(iconPath.c_str());
    if (icon) {
        SDL_SetWindowIcon(mWindow, icon);
        SDL_FreeSurface(icon);
    }
#endif

    RPG_LOG_INFO("Window created: " + std::to_string(mWidth) + "x" + std::to_string(mHeight));
    return true;
}

bool Window::CreateForeign(int width, int height) {
    // Kein SDL-Window, kein GL-Kontext - der Host (z.B. QOpenGLWidget im
    // Qt-Editor) besitzt Kontext & Swapping. Wir halten nur die Groesse.
    mWidth = width;
    mHeight = height;
    mForeign = true;
    RPG_LOG_INFO("Foreign window handle created: " + std::to_string(mWidth) + "x" + std::to_string(mHeight));
    return true;
}

void Window::SetForeignSize(int width, int height) {
    if (!mForeign) return;
    mWidth = width;
    mHeight = height;
}

void Window::Destroy() {
    if (mForeign) {
        // Host besitzt Fenster/Kontext - hier nichts zerstoeren.
        mForeign = false;
        mWidth = 0;
        mHeight = 0;
        RPG_LOG_INFO("Foreign window handle released");
        return;
    }
    if (mContext) {
        SDL_GL_DeleteContext(mContext);
        mContext = nullptr;
    }
    if (mWindow) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
    SDL_Quit();
    RPG_LOG_INFO("Window destroyed");
}

void Window::SwapBuffers() {
    if (mForeign) return; // Host (Qt) swapped selbst
    SDL_GL_SwapWindow(mWindow);
}

void Window::PollEvents() {
    if (mForeign) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                mShouldClose = true;
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    mWidth = e.window.data1;
                    mHeight = e.window.data2;
                    glViewport(0, 0, mWidth, mHeight);
                    RPG_LOG_DEBUG("Window resized to " + std::to_string(mWidth) + "x" + std::to_string(mHeight));
                }
                break;
        }
    }
}

void Window::SetTitle(const std::string& title) {
    if (mWindow) SDL_SetWindowTitle(mWindow, title.c_str());
}

void Window::SetVSync(bool enabled) {
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) != 0) {
        RPG_LOG_WARN(std::string("Failed to set VSync: ") + SDL_GetError());
    }
}

bool Window::SetFullscreen(bool fullscreen) {
    if (mForeign) {
        // Kein eigenes OS-Fenster (Qt-Host): der Host regelt das Widget.
        return false;
    }
    if (!mWindow) return false;
    if (SDL_SetWindowFullscreen(mWindow,
            fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        RPG_LOG_WARN(std::string("SetFullscreen fehlgeschlagen: ") + SDL_GetError());
        return false;
    }
    mFullscreen = fullscreen;
    // Groesse nachziehen (im Vollbild = Desktop-Aufloesung, sonst Restore)
    SDL_GetWindowSize(mWindow, &mWidth, &mHeight);
    RPG_LOG_INFO(std::string("Vollbild: ") + (mFullscreen ? "AN" : "AUS") +
                 " (" + std::to_string(mWidth) + "x" + std::to_string(mHeight) + ")");
    return true;
}

void Window::SetSize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    mWidth = width;
    mHeight = height;
    if (!mForeign && mWindow)
        SDL_SetWindowSize(mWindow, width, height);
}

} // namespace rpg
