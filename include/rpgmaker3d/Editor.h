#pragma once

#include <memory>
#include "Types.h"

namespace rpg {

class Engine;
class Window;
class Renderer;
class Scene;
class Project;
class Map;

class AudioManager;
class AudioPreview;

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

    void HandleSceneViewPicking();
    void PaintTileAt(int x, int z);

private:
    void DrawMenuBar();
    void DrawSceneView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawProjectPanel();
    void DrawMapEditor();
    void DrawScriptEditor();
    void DrawConsole();
    void DrawPrefabBrowser();
    void DrawLightingEditor();

    void InitializeDefaultLayout(unsigned int dockspaceId, float width, float height);

    void CreateCube();
    void CreatePlane();
    void CreateLight();
    void DeleteSelectedEntity();
    void SaveMap();
    void LoadMap();

    Engine& mEngine;
    std::unique_ptr<AudioPreview> mAudioPreview;
    bool mInitialized = false;
    bool mShowDemo = false;
    bool mPlayMode = false;
    bool mLayoutInitialized = false;
    int mSelectedEntity = -1;
    int mSelectedLayer = 0;
    int mSelectedTile = 0;
    int mPaintX = 0;
    int mPaintZ = 0;
    Vec2 mSceneViewPos{0.0f};
    Vec2 mSceneViewSize{1280.0f, 720.0f};
    float mTileScale = 2.0f;
    bool mSceneViewHovered = false;
    bool mSceneViewFocused = false;
};

} // namespace rpg
