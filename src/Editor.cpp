#include "rpgmaker3d/Editor.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#if defined(_WIN32)
#include <SDL.h>
#else
#include <SDL.h>
#endif

namespace rpg {

Editor::Editor(Engine& engine) : mEngine(engine) {
}

Editor::~Editor() {
    Shutdown();
}

bool Editor::Initialize(Window& window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window.GetNativeWindow(), window.GetGLContext());
    ImGui_ImplOpenGL3_Init("#version 330");

    mInitialized = true;
    return true;
}

void Editor::Shutdown() {
    if (!mInitialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    mInitialized = false;
}

void Editor::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Editor::DrawUI() {
    DrawMenuBar();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("DockSpace", nullptr, flags);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
    ImGui::PopStyleVar(2);

    DrawSceneView();
    DrawHierarchy();
    DrawInspector();
    DrawProjectPanel();
    DrawMapEditor();
    DrawScriptEditor();
    DrawConsole();

    if (mShowDemo) {
        ImGui::ShowDemoWindow(&mShowDemo);
    }
}

void Editor::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backupCurrentWindow = SDL_GL_GetCurrentWindow();
        SDL_GLContext backupCurrentContext = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupCurrentWindow, backupCurrentContext);
    }
}

bool Editor::WantCaptureInput() const {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}

void Editor::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project")) {
                mEngine.GetProject().New("./NewRPGProject", "New RPG");
            }
            if (ImGui::MenuItem("Open Project")) {
                mEngine.GetProject().Load("./SampleProject");
            }
            if (ImGui::MenuItem("Save Project")) {
                mEngine.GetProject().Save();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                mEngine.RequestQuit();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Demo Window", nullptr, &mShowDemo);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Play")) {
            if (ImGui::MenuItem(mPlayMode ? "Stop" : "Play", "F5")) {
                mPlayMode = !mPlayMode;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void Editor::DrawSceneView() {
    ImGui::Begin("Scene");
    ImVec2 size = ImGui::GetContentRegionAvail();
    mSceneViewSize = Vec2(size.x, size.y);

    // In einer vollständigen Engine würde hier ein Framebuffer-Texture gezeigt.
    ImGui::Text("Scene View (%.0f x %.0f)", size.x, size.y);
    ImGui::Text("WASD + Rechtsklick zum Navigieren");
    ImGui::End();
}

void Editor::DrawHierarchy() {
    ImGui::Begin("Hierarchy");
    Scene& scene = mEngine.GetScene();
    for (EntityID id : scene.GetEntities()) {
        bool selected = (mSelectedEntity == static_cast<int>(id));
        if (ImGui::Selectable(scene.GetEntityName(id).c_str(), selected)) {
            mSelectedEntity = static_cast<int>(id);
        }
    }
    if (ImGui::Button("Add Entity")) {
        scene.CreateEntity("New Entity");
    }
    ImGui::End();
}

void Editor::DrawInspector() {
    ImGui::Begin("Inspector");
    if (mSelectedEntity >= 0) {
        EntityID id = static_cast<EntityID>(mSelectedEntity);
        ImGui::Text("Entity: %s", mEngine.GetScene().GetEntityName(id).c_str());

        auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(id);
        if (transform) {
            if (ImGui::TreeNode("Transform")) {
                ImGui::DragFloat3("Position", &transform->transform.position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &transform->transform.rotation.x, 0.5f);
                ImGui::DragFloat3("Scale", &transform->transform.scale.x, 0.05f);
                ImGui::TreePop();
            }
        }
    } else {
        ImGui::Text("No entity selected");
    }
    ImGui::End();
}

void Editor::DrawProjectPanel() {
    ImGui::Begin("Project");
    ImGui::Text("Project: %s", mEngine.GetProject().GetInfo().name.c_str());
    ImGui::Text("Path: %s", mEngine.GetProject().GetProjectPath().c_str());
    ImGui::Separator();
    ImGui::Text("Assets:");
    ImGui::BulletText("textures/");
    ImGui::BulletText("models/");
    ImGui::BulletText("audio/");
    ImGui::BulletText("shaders/");
    ImGui::BulletText("maps/");
    ImGui::BulletText("scripts/");
    ImGui::End();
}

void Editor::DrawMapEditor() {
    ImGui::Begin("Map Editor");
    Map& map = mEngine.GetMap();

    if (ImGui::Button("Add Layer")) {
        map.AddLayer("Layer " + std::to_string(map.GetLayers().size()));
    }

    ImGui::Separator();
    ImGui::Text("Layers");
    int idx = 0;
    for (auto& layer : map.GetLayers()) {
        bool selected = (mSelectedLayer == idx);
        if (ImGui::Selectable(layer.name.c_str(), selected)) {
            mSelectedLayer = idx;
        }
        ++idx;
    }

    ImGui::Separator();
    ImGui::Text("Tile: %d", mSelectedTile);
    ImGui::SliderInt("Tile ID", &mSelectedTile, 0, 255);
    ImGui::End();
}

void Editor::DrawScriptEditor() {
    ImGui::Begin("Script Editor");
    ImGui::Text("Ruby Scripts");
    ImGui::Separator();
    static char scriptName[128] = "main.rb";
    ImGui::InputText("Script Name", scriptName, sizeof(scriptName));
    static char scriptContent[4096] =
        "# Main game script\n"
        "class Game\n"
        "  def initialize\n"
        "    @player = Actor.new(\"Hero\")\n"
        "  end\n"
        "end\n";
    ImGui::InputTextMultiline("Source", scriptContent, sizeof(scriptContent), ImVec2(-1, -1));
    if (ImGui::Button("Save Script")) {
        // Speichern Stub
    }
    ImGui::End();
}

void Editor::DrawConsole() {
    ImGui::Begin("Console");
    ImGui::Text("Engine ready. Waiting for commands...");
    ImGui::End();
}

} // namespace rpg
