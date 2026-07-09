#include <algorithm>
#include "rpgmaker3d/Editor.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/AudioPreview.h"
#include "rpgmaker3d/CommandHistory.h"
#include "rpgmaker3d/Command.h"
#include "rpgmaker3d/Prefab.h"
#include "rpgmaker3d/RubyVM.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#if defined(_WIN32)
#include <SDL.h>
#else
#include <SDL.h>
#endif

#include <filesystem>
#include <fstream>
#include <sstream>

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
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;

    ImGui_ImplSDL2_InitForOpenGL(window.GetNativeWindow(), window.GetGLContext());
    ImGui_ImplOpenGL3_Init("#version 330");

    mAudioPreview = std::make_unique<AudioPreview>(mEngine.GetAudio());

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

    if (!mLayoutInitialized) {
        InitializeDefaultLayout(dockspaceId, viewport->Size.x, viewport->Size.y);
        mLayoutInitialized = true;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);

    DrawSceneView();
    DrawHierarchy();
    DrawInspector();
    DrawProjectPanel();
    DrawMapEditor();
    DrawScriptEditor();
    if (mAudioPreview) mAudioPreview->DrawUI();
    DrawPrefabBrowser();
    DrawConsole();

    if (mShowDemo) {
        ImGui::ShowDemoWindow(&mShowDemo);
    }
}

void Editor::InitializeDefaultLayout(ImGuiID dockspaceId, float width, float height) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(width, height));

    ImGuiID dock_main_id = dockspaceId;
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.24f, nullptr, &dock_main_id);
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);

    ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
    ImGui::DockBuilderDockWindow("Map Editor", dock_id_left);
    ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
    ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Prefab Browser", dock_id_right);
    ImGui::DockBuilderDockWindow("Scene", dock_main_id);
    ImGui::DockBuilderDockWindow("Script Editor", dock_main_id);
    ImGui::DockBuilderFinish(dockspaceId);
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
            auto& history = mEngine.GetCommandHistory();
            std::string undoLabel = "Undo";
            std::string redoLabel = "Redo";
            if (history.CanUndo()) undoLabel += " (" + history.GetUndoName() + ")";
            if (history.CanRedo()) redoLabel += " (" + history.GetRedoName() + ")";

            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, history.CanUndo())) {
                history.Undo(mEngine);
                RPG_LOG_INFO("Undo: " + history.GetUndoName());
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, history.CanRedo())) {
                history.Redo(mEngine);
                RPG_LOG_INFO("Redo: " + history.GetRedoName());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Del")) {
                DeleteSelectedEntity();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Cube")) CreateCube();
            if (ImGui::MenuItem("Plane")) CreatePlane();
            if (ImGui::MenuItem("Light")) CreateLight();
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

        ImGui::Separator();
        ImGui::Text("FPS: %d", mEngine.GetFPS());

        ImGui::EndMainMenuBar();
    }
}

void Editor::DrawSceneView() {
    ImGui::Begin("Scene");
    ImVec2 size = ImGui::GetContentRegionAvail();
    mSceneViewSize = Vec2(size.x, size.y);

    unsigned int texId = mEngine.GetSceneTextureID();
    if (texId != 0) {
        ImTextureID img = (ImTextureID)(intptr_t)texId;
        ImGui::Image(img, size, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::Text("Scene View (%.0f x %.0f)", size.x, size.y);
        ImGui::Text("WASD + Rechtsklick zum Navigieren");
    }

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

        ImGui::Separator();
        if (ImGui::Button("Delete Entity", ImVec2(-1, 0))) {
            DeleteSelectedEntity();
        }

        static char prefabName[128] = "";
        ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
        if (ImGui::Button("Save as Prefab", ImVec2(-1, 0))) {
            Scene& scene = mEngine.GetScene();
            auto* transform = scene.GetComponent<TransformComponent>(id);
            auto* model = scene.GetComponent<ModelRendererComponent>(id);
            if (transform) {
                PrefabData data;
                data.name = prefabName[0] ? prefabName : scene.GetEntityName(id);
                data.position = transform->transform.position;
                data.rotation = transform->transform.rotation;
                data.scale = transform->transform.scale;
                if (model) {
                    data.hasModel = true;
                    data.color = model->color;
                }
                std::string path = Prefab::GetPrefabDirectory() + "/" + data.name + ".prefab";
                Prefab prefab;
                if (prefab.Save(path, data)) {
                    RPG_LOG_INFO("Saved prefab: " + path);
                }
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

    ImGui::Text("Assets");
    std::string basePath = mEngine.GetProject().GetProjectPath();
    if (basePath.empty()) basePath = ".";

    try {
        if (std::filesystem::exists(basePath + "/assets")) {
            for (const auto& entry : std::filesystem::directory_iterator(basePath + "/assets")) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    if (ImGui::TreeNode(name.c_str())) {
                        for (const auto& sub : std::filesystem::directory_iterator(entry.path())) {
                            ImGui::BulletText("%s", sub.path().filename().string().c_str());
                        }
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::BulletText("%s", name.c_str());
                }
            }
        } else {
            ImGui::Text("No assets folder found.");
        }
    } catch (...) {
        ImGui::Text("Could not read project folder.");
    }

    ImGui::End();
}


void Editor::DrawMapEditor() {
    ImGui::Begin("Map Editor");
    Map& map = mEngine.GetMap();

    if (ImGui::Button("Add Layer")) {
        map.AddLayer("Layer " + std::to_string(map.GetLayers().size()));
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Layer")) {
        for (int z = 0; z < map.GetHeight(); ++z) {
            for (int x = 0; x < map.GetWidth(); ++x) {
                map.SetTile(mSelectedLayer, x, z, -1);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Map")) {
        SaveMap();
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Map")) {
        LoadMap();
    }

    ImGui::Separator();
    ImGui::Text("Layers");
    int idx = 0;
    for (auto& layer : map.GetLayers()) {
        bool selected = (mSelectedLayer == idx);
        if (ImGui::Selectable((layer.name + "##" + std::to_string(idx)).c_str(), selected)) {
            mSelectedLayer = idx;
        }
        ++idx;
    }

    ImGui::Separator();
    ImGui::Text("Selected Tile: %d", mSelectedTile);
    ImGui::SliderInt("Tile ID", &mSelectedTile, 0, 255);

    ImGui::Separator();
    ImGui::Text("Paint Tile");
    ImGui::InputInt("X", &mPaintX);
    ImGui::InputInt("Z", &mPaintZ);
    if (ImGui::Button("Paint")) {
        Map& map = mEngine.GetMap();
        int oldTile = map.GetTile(mSelectedLayer, mPaintX, mPaintZ);
        auto cmd = std::make_shared<SetTileCommand>(mSelectedLayer, mPaintX, mPaintZ, oldTile, mSelectedTile);
        mEngine.GetCommandHistory().Execute(mEngine, cmd);
        RPG_LOG_INFO("Painted tile at (" + std::to_string(mPaintX) + ", " + std::to_string(mPaintZ) + ")");
    }

    // Visueller Tileset-Picker
    Tileset* tileset = map.GetTileset().get();
    if (tileset && tileset->GetTexture()) {
        ImGui::Separator();
        ImGui::Text("Tileset Picker");

        int cols = tileset->GetColumns();
        int rows = tileset->GetRows();
        if (cols > 0 && rows > 0) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float scale = mTileScale;
            float previewSize = std::min(avail.x / cols, 64.0f * scale);

            ImTextureID texId = (ImTextureID)(intptr_t)(tileset->GetTexture()->GetID());

            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    int id = y * cols + x;
                    Vec4 uv = tileset->GetTileUV(id);

                    ImVec2 uv0(uv.x, uv.y);
                    ImVec2 uv1(uv.z, uv.w);
                    ImVec2 size(previewSize, previewSize);

                    ImGui::PushID(id);
                    if (id == mSelectedTile) {
                        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
                    }

                    if (ImGui::ImageButton("tile", texId, size, uv0, uv1)) {
                        mSelectedTile = id;
                    }

                    if (id == mSelectedTile) {
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopID();

                    if (x < cols - 1) ImGui::SameLine();
                }
            }
        }
    } else {
        ImGui::Text("No tileset loaded.");
    }

    ImGui::End();
}

void Editor::DrawScriptEditor() {
    ImGui::Begin("Script Editor");
    ImGui::Text("Ruby Scripts");
    ImGui::Separator();

    static char scriptName[128] = "main.rb";
    static char scriptContent[4096] =
        "# Main game script\n"
        "class Game\n"
        "  def initialize\n"
        "    @player = Actor.new(\"Hero\")\n"
        "    @player.move_to(0, 0, 0)\n"
        "  end\n"
        "\n"
        "  def update(delta_time)\n"
        "    @player.move(0, 0, delta_time) if Input.key_down?(:w)\n"
        "    @player.move(0, 0, -delta_time) if Input.key_down?(:s)\n"
        "  end\n"
        "end\n"
        "\n"
        "$game = Game.new\n";

    ImGui::InputText("Script Name", scriptName, sizeof(scriptName));
    ImGui::InputTextMultiline("Source", scriptContent, sizeof(scriptContent), ImVec2(-1, -1));

    if (ImGui::Button("Save Script")) {
        std::string path = mEngine.GetProject().GetScriptPath(scriptName);
        std::ofstream file(path);
        if (file.is_open()) {
            file << scriptContent;
            file.close();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Saved!");
        } else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to save!");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Run Script")) {
        if (mEngine.GetRubyVM().ExecuteString(scriptContent)) {
            RPG_LOG_INFO("Script executed successfully");
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Executed!");
        } else {
            RPG_LOG_ERROR("Script execution failed");
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed! (Ruby not compiled in or error)");
        }
    }
    ImGui::End();
}

static ImVec4 GetLogColor(rpg::LogLevel level) {
    switch (level) {
        case rpg::LogLevel::Trace:   return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        case rpg::LogLevel::Debug:   return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
        case rpg::LogLevel::Info:    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        case rpg::LogLevel::Warning: return ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
        case rpg::LogLevel::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        case rpg::LogLevel::Fatal:   return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

void Editor::DrawConsole() {
    ImGui::Begin("Console");

    static char command[256] = "";
    static bool autoScroll = true;

    ImGui::Text("Engine ready. FPS: %d", mEngine.GetFPS());
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::Separator();

    const auto& entries = rpg::Logger::Get().GetEntries();

    ImGui::BeginChild("Log", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : entries) {
        ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(entry.level));
        std::string line = "[" + std::to_string(static_cast<int>(entry.time * 100) / 100.0f) + "] "
                         + "[" + std::string(rpg::Logger::LevelToString(entry.level)) + "] ";
        if (!entry.source.empty()) {
            line += "[" + entry.source + "] ";
        }
        line += entry.message;
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    if (ImGui::InputText("Command", command, sizeof(command), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string cmd = command;
        rpg::Logger::Get().Info("> " + cmd, "Console");

        if (cmd == "clear") {
            rpg::Logger::Get().Clear();
        } else if (cmd == "fps") {
            rpg::Logger::Get().Info("FPS: " + std::to_string(mEngine.GetFPS()), "Console");
        } else if (cmd == "help") {
            rpg::Logger::Get().Info("Commands: clear, fps, help, loglevel", "Console");
        } else if (cmd == "loglevel") {
            rpg::Logger::Get().Info("Log levels: Trace, Debug, Info, Warning, Error, Fatal", "Console");
        } else {
            rpg::Logger::Get().Warning("Unknown command: " + cmd, "Console");
        }
        command[0] = '\0';
    }
    ImGui::End();
}

void Editor::CreateCube() {
    Scene& scene = mEngine.GetScene();
    EntityID id = scene.CreateEntity("Cube");
    auto* transform = scene.AddComponent<TransformComponent>(id);
    transform->transform.position = Vec3(0, 0.5f, 0);
    auto* model = scene.AddComponent<ModelRendererComponent>(id);
    model->model = std::make_shared<Model>();
    model->model->AddMesh(MeshFactory::CreateCube(1.0f));
    model->color = Color(1.0f);
    mSelectedEntity = static_cast<int>(id);
    RPG_LOG_INFO("Created Cube entity");
}

void Editor::CreatePlane() {
    Scene& scene = mEngine.GetScene();
    EntityID id = scene.CreateEntity("Plane");
    auto* transform = scene.AddComponent<TransformComponent>(id);
    transform->transform.position = Vec3(0, 0.1f, 0);
    auto* model = scene.AddComponent<ModelRendererComponent>(id);
    model->model = std::make_shared<Model>();
    model->model->AddMesh(MeshFactory::CreatePlane(2.0f));
    model->color = Color(1.0f);
    mSelectedEntity = static_cast<int>(id);
    RPG_LOG_INFO("Created Plane entity");
}

void Editor::CreateLight() {
    Scene& scene = mEngine.GetScene();
    EntityID id = scene.CreateEntity("Light");
    auto* transform = scene.AddComponent<TransformComponent>(id);
    transform->transform.position = Vec3(0, 3.0f, 0);
    auto* model = scene.AddComponent<ModelRendererComponent>(id);
    model->model = std::make_shared<Model>();
    model->model->AddMesh(MeshFactory::CreateCube(0.3f));
    model->color = Color(1.0f, 1.0f, 0.0f, 1.0f);
    mSelectedEntity = static_cast<int>(id);
    RPG_LOG_INFO("Created Light entity");
}

void Editor::DeleteSelectedEntity() {
    if (mSelectedEntity >= 0) {
        mEngine.GetScene().DestroyEntity(static_cast<EntityID>(mSelectedEntity));
        mSelectedEntity = -1;
        RPG_LOG_INFO("Deleted selected entity");
    }
}

void Editor::SaveMap() {
    std::string path = mEngine.GetProject().GetMapPath(1);
    mEngine.GetMap().Save(path);
    RPG_LOG_INFO("Map saved to: " + path);
}

void Editor::LoadMap() {
    std::string path = mEngine.GetProject().GetMapPath(1);
    if (mEngine.GetMap().Load(path)) {
        RPG_LOG_INFO("Map loaded from: " + path);
    } else {
        RPG_LOG_ERROR("Failed to load map: " + path);
    }
}

void Editor::DrawPrefabBrowser() {
    ImGui::Begin("Prefab Browser");

    std::string dir = Prefab::GetPrefabDirectory();
    try {
        std::filesystem::create_directories(dir);
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() != ".prefab") continue;

            std::string name = entry.path().stem().string();
            if (ImGui::Button(("Load ##" + name).c_str())) {
                PrefabData data;
                Prefab prefab;
                if (prefab.Load(entry.path().string(), data)) {
                    Scene& scene = mEngine.GetScene();
                    EntityID id = scene.CreateEntity(data.name);
                    auto* transform = scene.AddComponent<TransformComponent>(id);
                    transform->transform.position = data.position;
                    transform->transform.rotation = data.rotation;
                    transform->transform.scale = data.scale;

                    if (data.hasModel) {
                        auto* model = scene.AddComponent<ModelRendererComponent>(id);
                        model->model = std::make_shared<Model>();
                        if (data.modelType == "plane") {
                            model->model->AddMesh(MeshFactory::CreatePlane(2.0f));
                        } else {
                            model->model->AddMesh(MeshFactory::CreateCube(1.0f));
                        }
                        model->color = data.color;
                    }

                    mSelectedEntity = static_cast<int>(id);
                    RPG_LOG_INFO("Loaded prefab: " + data.name);
                }
            }
            ImGui::SameLine();
            ImGui::Text("%s", name.c_str());
        }
    } catch (...) {
        ImGui::Text("Could not read prefabs folder.");
    }

    ImGui::End();
}

} // namespace rpg
