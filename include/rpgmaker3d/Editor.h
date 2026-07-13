#pragma once

#include <memory>
#include <string>
#include <functional>
#include "Types.h"
#include "EventSystem.h"
#include "EditorStyle.h"

namespace rpg {

class Engine;
class Window;
class Renderer;
class Scene;
class Project;
class Map;

class AudioManager;
class AudioPreview;
class EditorToolbar;

enum class GizmoMode {
    None,
    Translate,
    Rotate,
    Scale
};

enum class GizmoSpace {
    Local,
    World
};

struct CrashInfo {
    std::string message;
    std::string stackTrace;
    std::string timestamp;
};

class Editor {
public:
    Editor(Engine& engine);
    ~Editor();

    bool Initialize(Window& window);
    void Shutdown();

    void BeginFrame();
    void DrawUI();
    void EndFrame();

    bool WantCaptureInput() const;
    bool IsPlaying() const { return mPlayMode; }
    void SetPlayMode(bool play) { mPlayMode = play; }
    Vec2 GetSceneViewSize() const { return mSceneViewSize; }
    Vec2 GetSceneViewPos() const { return mSceneViewPos; }
    int GetSelectedEntity() const { return mSelectedEntity; }
    bool IsSceneViewHovered() const { return mSceneViewHovered; }
    bool IsSceneViewFocused() const { return mSceneViewFocused; }

    void HandleSceneViewPicking();
    void PaintTileAt(int x, int z);

    // Crash handling
    static void SetCrashCallback(std::function<void(const CrashInfo&)> callback);
    static void HandleCrash(const std::string& message, const std::string& stackTrace = "");
    void ShowCrashDialog();

private:
    void DrawMenuBar();
    void DrawSceneView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawProjectPanel();
    void DrawMapEditor();
    void DrawEventEditor();
    void DrawScriptEditor();
    void DrawConsole();
    void DrawPrefabBrowser();
    void DrawLightingEditor();
    void DrawEnvironmentEditor();
    void DrawGizmo();  // NEW: Gizmo rendering
    void DrawToolbar(); // NEW: Toolbar with gizmo controls
    void DrawStatusBar(); // NEW: Status bar
    void HandleShortcuts(); // NEW: Keyboard shortcuts
    void HandleSceneViewCamera();
    void HandleSceneViewContextMenu(const ImVec2& viewPos, const ImVec2& viewSize);

    void InitializeDefaultLayout(unsigned int dockspaceId, float width, float height);

    void CreateCube();
    void CreatePlane();
    void CreateLight();
    void DeleteSelectedEntity();
    void SaveMap();
    void LoadMap();

    // File dialogs
    std::string OpenFileDialog(const char* filter = "All Files (*.*)\0*.*\0");
    std::string SaveFileDialog(const char* filter = "All Files (*.*)\0*.*\0");
    std::string SelectFolderDialog();

    // Gizmo functions
    void UpdateGizmo();
    void DrawGizmoAxis(const Vec3& position, const Mat4& view, const Mat4& proj, const Vec2& viewPos, const Vec2& viewSize);
    bool GizmoIntersect(const Vec2& mousePos, const Vec2& viewPos, const Vec2& viewSize, Vec3& outAxis);

    // Map Editor Funktionen
    void LoadSelectedMap();
    void ResizeCurrentMap(int width, int height);
    void LoadTilesetForMap(int tilesetId);

    // Event Editor Funktionen
    void DrawEventCommandList(EventPage& page);
    void DrawAddEventCommand(EventPage& page);
    void AddCommand(EventPage& page, EventCommandCode code);
    std::string GetEventCommandName(EventCommandCode code);
    std::string GetEventCommandParamsString(const EventCommand& cmd);

    // Crash handling
    static void CrashCallback(const CrashInfo& info);
    CrashInfo mLastCrashInfo;
    bool mShowCrashDialog = false;

    Engine& mEngine;
    std::unique_ptr<AudioPreview> mAudioPreview;
    std::unique_ptr<EditorToolbar> mToolbar;
    bool mInitialized = false;
    bool mShowDemo = false;
    bool mPlayMode = false;
    bool mLayoutInitialized = false;
    int mSelectedEntity = -1;
    int mSelectedLayer = 0;
    int mSelectedTile = 0;
    int mPaintX = 0;
    int mPaintZ = 0;
    int mSelectedMapIndex = -1;  // Für Map-Liste
    int mSelectedEventId = -1;   // Für Event-Editor
    int mSelectedEventPage = -1; // Aktuelle Event-Seite
    int mSelectedCommandIndex = -1; // Ausgewählter Befehl
    int mEditingCommandIndex = -1;  // Bearbeiteter Befehl
    bool mShowCommandEditor = false;
    EventCommand mClipboardCommand; // Für Kopieren/Einfügen
    Vec2 mSceneViewPos{0.0f};
    Vec2 mSceneViewSize{1280.0f, 720.0f};
    float mTileScale = 2.0f;
    bool mSceneViewHovered = false;
    bool mSceneViewFocused = false;
    float mTimeOfDay = 12.0f;
    float mTimeOfDaySpeed = 1.0f;
    EditorTheme mCurrentTheme = EditorTheme::Dark;

    // Gizmo state
    GizmoMode mGizmoMode = GizmoMode::Translate;
    GizmoSpace mGizmoSpace = GizmoSpace::Local;
    bool mGizmoActive = false;
    bool mGizmoSnap = true;
    float mGizmoSnapValue = 0.5f;
    int mGizmoAxis = -1; // 0=X, 1=Y, 2=Z
    Vec3 mGizmoStartPos;
    Vec3 mGizmoStartRot;
    Vec3 mGizmoStartScale;
    Vec2 mGizmoStartMousePos;
    bool mShowGrid = true;
    bool mContextMenuPending = false;
    ImVec2 mContextMenuMousePos{0, 0};
};

} // namespace rpg
