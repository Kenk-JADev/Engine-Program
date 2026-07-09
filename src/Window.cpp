#include "rpgmaker3d/Window.h"
#include <iostream>

#if defined(_WIN32)
#include <SDL.h>
#else
#include <SDL.h>
#endif

#include <glad/gl.h>

namespace rpg {

Window::Window() = default;

Window::~Window() {
    Destroy();
}

bool Window::Create(const std::string& title, int width, int height, bool editorMode) {
    mWidth = width;
    mHeight = height;
    mEditorMode = editorMode;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (editorMode) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    mWindow = SDL_CreateWindow(title.c_str(),
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               mWidth, mHeight, flags);
    if (!mWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    mContext = SDL_GL_CreateContext(mWindow);
    if (!mContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        return false;
    }

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (version == 0) {
        std::cerr << "gladLoadGL failed" << std::endl;
        return false;
    }

    SetVSync(true);

    glViewport(0, 0, mWidth, mHeight);
    return true;
}

void Window::Destroy() {
    if (mContext) {
        SDL_GL_DeleteContext(mContext);
        mContext = nullptr;
    }
    if (mWindow) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
    SDL_Quit();
}

void Window::SwapBuffers() {
    SDL_GL_SwapWindow(mWindow);
}

void Window::PollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                mShouldClose = true;
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    mWidth = e.window.data1;
                    mHeight = e.window.data2;
                    glViewport(0, 0, mWidth, mHeight);
                }
                break;
        }
    }
}

void Window::SetTitle(const std::string& title) {
    SDL_SetWindowTitle(mWindow, title.c_str());
}

void Window::SetVSync(bool enabled) {
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

} // namespace rpg
