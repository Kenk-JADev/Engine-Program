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

    // Gibt an, ob Initialize()/InitializeEmbedded() erfolgreich durchlaufen
    // wurde. WICHTIG: Die Qt-Docks rufen refresh() im Konstruktor auf, der
    // Engine-Subsysteme dereferenziert. Vor der Initialisierung sind diese
    // noch nullptr -> ohne diesen Guard käme es zu einem Null-Dereferenz-
    // Absturz, bevor das Editor-Fenster überhaupt sichtbar wird.
    bool IsInitialized() const { return mInitialized; }

    Window& GetWindow() { return *mWindow; }
    Renderer& GetRenderer() { return *mRenderer; }
    Input& GetInput() { return *mInput; }
    AudioManager& GetAudio() { return *mAudio; }
    Scene& GetScene() { return *mScene; }
    Project& GetProject() { return *mProject; }
    Map& GetMap() { return *mMap; }
    ResourceManager& GetResources() { return *mResources; }

    // ImGui-Editor ist entfernt. GetEditor() bleibt als Stub fuer Alt-Code
    // und liefert immer nullptr.
    void* GetEditor() { return nullptr; }

    CommandHistory& GetCommandHistory() { return *mCommandHistory; }
    RubyVM& GetRubyVM() { return *mRubyVM; }
    ScriptManager& GetScriptManager() { return *mScriptManager; }
    #ifdef RPGMAKER3D_ENABLE_RMLUI
    RmlUiSystem* GetRmlUi() { return mRmlUi.get(); }
#else
    RmlUiSystem* GetRmlUi() { return nullptr; }
#endif

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
    /// XP-Titelbildschirm (Player): Neues Spiel / Weiterspielen / Beenden.
    /// Verdrahtet die TitleScreen-Callbacks und zeigt den Titel an.
    void StartTitleMode();
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

    // Editor-Selektion (Qt-Host / externer Host). -1 = keine.
    void SetSelectedEntity(int id) { mSelectedEntity = id; }
    int GetSelectedEntity() const { return mSelectedEntity; }

private:
    bool InitializeInternal(const std::string& title, int width, int height, bool editorMode, bool createOsWindow);
    /// Event-Audio (BGM/BGS/ME/SE) + Karten-Autoplay: loest Dateinamen gegen
    /// <Projekt>/Audio/<Art>/ (XP-Struktur) und assets/audio/<Art>/ auf und
    /// spielt ueber den AudioManager ab. kind: 0=BGM,1=BGS,2=ME,3=SE.
    void PlayEventAudio(const std::string& name, int kind, bool loop);
    std::string ResolveAudioPath(const std::string& name, int kind) const;
    /// Bild-Pfadaufloesung fuer UI.show_picture + Titelgrafik:
    /// <Projekt>/Graphics/Pictures|Titles/ (XP), Pictures/, assets/…
    std::string ResolvePicturePathFor(const std::string& filename) const;
    /// Titelmodus beenden (Titelgrafik entfernen, Titel-BGM ausblenden)
    void EndTitleMode();
    /// „Zum Titelbildschirm" aus dem Menue: Spiel sauber stoppen + Titel
    void ReturnToTitle();
    std::unique_ptr<Window> mWindow;
    std::unique_ptr<Renderer> mRenderer;
    std::unique_ptr<Input> mInput;
    std::unique_ptr<AudioManager> mAudio;
    std::unique_ptr<Scene> mScene;
    std::unique_ptr<Project> mProject;
    std::unique_ptr<Map> mMap;
    std::unique_ptr<ResourceManager> mResources;

    std::unique_ptr<Framebuffer> mSceneFramebuffer;
    std::unique_ptr<CommandHistory> mCommandHistory;
    std::unique_ptr<RubyVM> mRubyVM;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    std::unique_ptr<RmlUiSystem> mRmlUi;
#endif
    std::unique_ptr<ScriptManager> mScriptManager;
    Mesh mGridMesh;

    bool mRunning = false;
    bool mInitialized = false;
    bool mEditorMode = true;
    bool mPlayMode = false;
    bool mPlayModeFollowPlayer = true;
    bool mShowGrid = true;
    Vec2 mSceneViewPos{0.0f};
    Vec2 mSceneViewSize{1280.0f, 720.0f};
    EntityID mActiveCameraEntity = INVALID_ENTITY;
    int mSelectedEntity = -1;
    float mDeltaTime = 0.0f;
    float mTime = 0.0f;
    int mFPS = 0;
    float mFPSTimer = 0.0f;
    int mFrameCount = 0;

    // XP-Kampfstatus-Anzeige (Gegner-/Gruppenzeile oben, Screen-Text-Ids)
    int mBattleStatusEnemiesId = -1;
    int mBattleStatusPartyId = -1;
    float mBattleStatusTimer = 0.0f;
};

} // namespace rpg
