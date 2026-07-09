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

    bool IsEditorMode() const { return mEditorMode; }

private:
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mContext = nullptr;
    int mWidth = 1280;
    int mHeight = 720;
    bool mShouldClose = false;
    bool mEditorMode = true;
};

} // namespace rpg
