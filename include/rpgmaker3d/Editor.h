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

private:
    void DrawMenuBar();
    void DrawSceneView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawProjectPanel();
    void DrawMapEditor();
    void DrawScriptEditor();
    void DrawConsole();

    Engine& mEngine;
    bool mInitialized = false;
    bool mShowDemo = false;
    bool mPlayMode = false;
    int mSelectedEntity = -1;
    int mSelectedLayer = 0;
    int mSelectedTile = 0;
    Vec2 mSceneViewSize{0.0f};
};

} // namespace rpg
