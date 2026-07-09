#pragma once

#include <memory>
#include <string>
#include "Types.h"

namespace rpg {

class Window;
class Renderer;
class Input;
class AudioManager;
class Scene;
class Project;
class Map;
class ResourceManager;
class Editor;

class Engine {
public:
    Engine();
    ~Engine();

    bool Initialize(const std::string& title, int width, int height, bool editorMode = true);
    void Shutdown();

    void Run();
    void Update(float dt);
    void Render();

    bool IsRunning() const { return mRunning; }
    void RequestQuit() { mRunning = false; }

    Window& GetWindow() { return *mWindow; }
    Renderer& GetRenderer() { return *mRenderer; }
    Input& GetInput() { return *mInput; }
    AudioManager& GetAudio() { return *mAudio; }
    Scene& GetScene() { return *mScene; }
    Project& GetProject() { return *mProject; }
    Map& GetMap() { return *mMap; }
    ResourceManager& GetResources() { return *mResources; }
    Editor* GetEditor() { return mEditor.get(); }

    float GetDeltaTime() const { return mDeltaTime; }
    float GetTime() const { return mTime; }

    void SetEditorMode(bool enabled) { mEditorMode = enabled; }
    bool IsEditorMode() const { return mEditorMode; }

private:
    std::unique_ptr<Window> mWindow;
    std::unique_ptr<Renderer> mRenderer;
    std::unique_ptr<Input> mInput;
    std::unique_ptr<AudioManager> mAudio;
    std::unique_ptr<Scene> mScene;
    std::unique_ptr<Project> mProject;
    std::unique_ptr<Map> mMap;
    std::unique_ptr<ResourceManager> mResources;
    std::unique_ptr<Editor> mEditor;

    bool mRunning = false;
    bool mEditorMode = true;
    float mDeltaTime = 0.0f;
    float mTime = 0.0f;
};

} // namespace rpg
