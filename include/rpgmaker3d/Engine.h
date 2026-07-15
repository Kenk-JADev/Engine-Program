#pragma once

#include <memory>
#include <string>
#include "Types.h"
#include "Model.h"

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
class Framebuffer;
class CommandHistory;
class RubyVM;
class ScriptManager;
class RmlUiSystem;

class MeshFactory;

class Engine {
public:
    Engine();
    ~Engine();

    bool Initialize(const std::string& title, int width, int height, bool editorMode = true);
    // Eingebetteter Modus (Qt-Editor): kein SDL-Fenster; GL-Kontext + glad
    // muessen VOR dem Aufruf bereits current sein. Update(dt)/Render() werden
    // dann vom Host getrieben (kein Run()).
    bool InitializeEmbedded(int width, int height, bool editorMode = true);
    void Shutdown();

    void Run();
    void Update(float dt);
    void Render();
    void RenderScene();

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

#ifdef RPGMAKER3D_BUILD_EDITOR
    Editor* GetEditor() { return mEditor.get(); }
#else
    // Der Standalone-Player wird ohne Editor.cpp gebaut.
    Editor* GetEditor() { return nullptr; }
#endif

    CommandHistory& GetCommandHistory() { return *mCommandHistory; }
    RubyVM& GetRubyVM() { return *mRubyVM; }
    ScriptManager& GetScriptManager() { return *mScriptManager; }

    float GetDeltaTime() const { return mDeltaTime; }
    float GetTime() const { return mTime; }
    int GetFPS() const { return mFPS; }
    unsigned int GetSceneTextureID() const;

    void SetEditorMode(bool enabled) { mEditorMode = enabled; }
    bool IsEditorMode() const { return mEditorMode; }

    void SetSceneViewRect(const Vec2& pos, const Vec2& size) {
        mSceneViewPos = pos;
        mSceneViewSize = size;
    }

    Vec2 GetSceneViewPos() const { return mSceneViewPos; }
    Vec2 GetSceneViewSize() const { return mSceneViewSize; }

    bool IsPlaying() const { return mPlayMode; }
    void SetPlaying(bool playing);
    bool IsPlayModeFollowPlayer() const { return mPlayModeFollowPlayer; }
    void SetPlayModeFollowPlayer(bool follow) { mPlayModeFollowPlayer = follow; }

    void SaveScene(const std::string& path) const;
    bool LoadScene(const std::string& path);

    void SetActiveCamera(EntityID cameraEntity) {
        mActiveCameraEntity = cameraEntity;
    }

    EntityID GetActiveCamera() const {
        return mActiveCameraEntity;
    }

    bool IsGridVisible() const { return mShowGrid; }
    void SetGridVisible(bool visible) { mShowGrid = visible; }
    void ToggleGrid() { mShowGrid = !mShowGrid; }

private:
    bool InitializeInternal(const std::string& title, int width, int height, bool editorMode, bool createOsWindow);
    std::unique_ptr<Window> mWindow;
    std::unique_ptr<Renderer> mRenderer;
    std::unique_ptr<Input> mInput;
    std::unique_ptr<AudioManager> mAudio;
    std::unique_ptr<Scene> mScene;
    std::unique_ptr<Project> mProject;
    std::unique_ptr<Map> mMap;
    std::unique_ptr<ResourceManager> mResources;

#ifdef RPGMAKER3D_BUILD_EDITOR
    std::unique_ptr<Editor> mEditor;
#endif

    std::unique_ptr<Framebuffer> mSceneFramebuffer;
    std::unique_ptr<CommandHistory> mCommandHistory;
    std::unique_ptr<RubyVM> mRubyVM;
    std::unique_ptr<RmlUiSystem> mRmlUi;   // RmlUi UI-System (PoC: ImGui-Nachfolger)
    std::unique_ptr<ScriptManager> mScriptManager;
    Mesh mGridMesh;

    bool mRunning = false;
    bool mEditorMode = true;
    bool mPlayMode = false;
    bool mImGuiInitialized = false;
    bool mPlayModeFollowPlayer = true;
    bool mShowGrid = true;
    Vec2 mSceneViewPos{0.0f};
    Vec2 mSceneViewSize{1280.0f, 720.0f};
    EntityID mActiveCameraEntity = INVALID_ENTITY;
    float mDeltaTime = 0.0f;
    float mTime = 0.0f;
    int mFPS = 0;
    float mFPSTimer = 0.0f;
    int mFrameCount = 0;
};

} // namespace rpg
