// LEGACY: ImGui-Editor – nicht mehr im CMake-Build (Qt-Editor ist Standard).
#include <algorithm>
#include <cmath>
#include <functional>
#include <cstring>
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
#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/EditorToolbar.h"
#include "rpgmaker3d/EditorStyle.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Input.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#if defined(_WIN32)
#include <SDL.h>
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#else
#include <SDL.h>
#include <gtk/gtk.h>
#endif

// filesystem support with MSVC fallback
#if defined(_MSC_VER) && _MSC_VER < 1920
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif

#include <fstream>
#include <sstream>
#include <array>
#include <chrono>
#include <iomanip>

namespace rpg {

// Static crash callback
std::function<void(const CrashInfo&)> g_CrashCallback = nullptr;

void Editor::SetCrashCallback(std::function<void(const CrashInfo&)> callback) {
    g_CrashCallback = callback;
}

void Editor::HandleCrash(const std::string& message, const std::string& stackTrace) {
    CrashInfo info;
    info.message = message;
    info.stackTrace = stackTrace;
    info.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    if (g_CrashCallback) {
        g_CrashCallback(info);
    }
}

void Editor::CrashCallback(const CrashInfo& info) {
    // This will be called from the static callback
    // We can't directly access member variables, so we store it globally
}

Editor::Editor(Engine& engine) : mEngine(engine), mToolbar(std::make_unique<EditorToolbar>(engine)) {
}

Editor::~Editor() {
    Shutdown();
}

bool Editor::Initialize(Window& window) {
    (void)window;
    // ImGui wird von Engine initialisiert – Editor nutzt nur bestehenden Context
    mAudioPreview = std::make_unique<AudioPreview>(mEngine.GetAudio());
    mToolbar->Initialize();
    
    // Initialize editor style
    EditorStyle::Initialize(EditorTheme::Dark);
    
    // Set crash callback
    Editor::SetCrashCallback([this](const CrashInfo& info) {
        mLastCrashInfo = info;
        mShowCrashDialog = true;
    });
    
    mInitialized = true;
    return true;
}

void Editor::Shutdown() {
    if (!mInitialized) return;
    mToolbar->Shutdown();
    mAudioPreview.reset();
    mInitialized = false;
}

void Editor::BeginFrame() {
    // ImGui Frame wird von Engine verwaltet
}

void Editor::DrawUI() {
    // Apply editor theme
    EditorStyle::ApplyTheme(mCurrentTheme);
    // Tageszeit läuft NUR im Playtest (Spielzeit), nicht im Editor-Idle
    // Damit Editor nicht ablenkt und Performance spart, und Spiel zeitabhängiges Lighting hat
    if (mTimeOfDaySpeed > 0.0f && mEngine.IsPlaying()) {
        Lighting::Get().UpdateTimeOfDay(mEngine.GetDeltaTime(), mTimeOfDaySpeed);
        mTimeOfDay = Lighting::Get().GetTimeOfDay();
    }

    // Im Playtest soll Editor nicht nutzbar sein - nur noch Scene + HUD
    // Alle Edit-Operationen werden geblockt (s. HandleSceneViewPicking/Camera/Shortcuts)
    // Wir zeigen trotzdem DockSpace, aber kennzeichnen Play-Modus prominent

    // Handle global shortcuts
    HandleShortcuts();

    // Show crash dialog if needed
    ShowCrashDialog();

    DrawMenuBar();

    // Toolbar nur im Editor-Modus, nicht im Playtest (verhindert dass UI durch Scene geht)
    if (!mEngine.IsPlaying() && mToolbar) mToolbar->Draw();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    // NoBackground entfernt – verhindert Flickern beim Fenster verschieben
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, flags);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace_v3");
    // PassthruCentralNode entfernt – war Hauptursache für Flickern
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (!mLayoutInitialized) {
        InitializeDefaultLayout(dockspaceId, viewport->Size.x, viewport->Size.y);
        mLayoutInitialized = true;
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    DrawSceneView();
    DrawHierarchy();
    DrawInspector();
    DrawProjectPanel();
    DrawMapEditor();
    DrawEventEditor();
    DrawScriptEditor();
    if (mAudioPreview) mAudioPreview->DrawUI();
    DrawPrefabBrowser();
    DrawLightingEditor();
    DrawEnvironmentEditor();
    DrawConsole();

    // Draw gizmo in scene view

    // Draw status bar
    DrawStatusBar();

    if (mShowDemo) {
        ImGui::ShowDemoWindow(&mShowDemo);
    }
}

void Editor::InitializeDefaultLayout(ImGuiID dockspaceId, float width, float height) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(width, height));

    // Clean layout matching actual window titles:
    // Left Hierarchy | Center Scene | Right Inspector
    // Bottom: Projekt / Konsole / Script Editor
    ImGuiID dock_main = dockspaceId;
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.18f, nullptr, &dock_main);
    ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.22f, nullptr, &dock_main);
    ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.22f, nullptr, &dock_main);
    ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.40f, nullptr, &dock_left);

    ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow("Karten-Editor", dock_left_bottom);
    ImGui::DockBuilderDockWindow("Event-Editor", dock_left_bottom);

    ImGui::DockBuilderDockWindow("Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Beleuchtung", dock_right);
    ImGui::DockBuilderDockWindow("Umgebung", dock_right);
    ImGui::DockBuilderDockWindow("Prefab Browser", dock_right);

    ImGui::DockBuilderDockWindow("Projekt", dock_bottom);
    ImGui::DockBuilderDockWindow("Konsole", dock_bottom);
    ImGui::DockBuilderDockWindow("Debug / Status", dock_bottom);
    ImGui::DockBuilderDockWindow("Audio Vorschau", dock_bottom);

    // Script/Event as free windows (also dockable if user wants)
    ImGui::DockBuilderDockWindow("Script Editor", dock_bottom);
    ImGui::DockBuilderDockWindow("Event-Editor", dock_left_bottom);

    ImGui::DockBuilderDockWindow("Scene", dock_main);
    ImGui::DockBuilderFinish(dockspaceId);
}

void Editor::EndFrame() {
    // ImGui Render wird von Engine gemacht
}

bool Editor::WantCaptureInput() const {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}


void Editor::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Datei")) {
            if (ImGui::MenuItem("Neues Projekt", "Ctrl+N")) {
                mEngine.GetProject().New("./NewRPGProject", "Neues RPG");
            }
            if (ImGui::MenuItem("Projekt öffnen...", "Ctrl+O")) {
                std::string path = SelectFolderDialog();
                if (!path.empty()) {
                    mEngine.GetProject().Load(path);
                }
            }
            if (ImGui::MenuItem("Projekt speichern", "Ctrl+S")) {
                mEngine.GetProject().Save();
                SaveMap();
            }
            if (ImGui::MenuItem("Projekt speichern unter...")) {
                std::string path = SelectFolderDialog();
                if (!path.empty()) {
                    mEngine.GetProject().SaveAs(path);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Szene speichern", "Ctrl+Shift+S")) {
                std::string path = SaveFileDialog("JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0");
                if (!path.empty()) {
                    mEngine.SaveScene(path);
                }
            }
            if (ImGui::MenuItem("Szene laden...", "Ctrl+Shift+O")) {
                std::string path = OpenFileDialog("JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0");
                if (!path.empty()) {
                    mEngine.LoadScene(path);
                    mSelectedEntity = -1;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Beenden", "Alt+F4")) {
                mEngine.RequestQuit();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Bearbeiten")) {
            auto& history = mEngine.GetCommandHistory();
            std::string undoLabel = "Rückgängig";
            std::string redoLabel = "Wiederholen";
            if (history.CanUndo()) undoLabel += " (" + history.GetUndoName() + ")";
            if (history.CanRedo()) redoLabel += " (" + history.GetRedoName() + ")";

            if (ImGui::MenuItem(undoLabel.c_str(), "Strg+Z", false, history.CanUndo())) {
                history.Undo(mEngine);
                RPG_LOG_INFO("Rückgängig: " + history.GetUndoName());
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Strg+Y", false, history.CanRedo())) {
                history.Redo(mEngine);
                RPG_LOG_INFO("Wiederholen: " + history.GetRedoName());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Ausgewähltes löschen", "Entf")) {
                DeleteSelectedEntity();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Erstellen")) {
            if (ImGui::MenuItem("Würfel")) CreateCube();
            if (ImGui::MenuItem("Ebene")) CreatePlane();
            if (ImGui::MenuItem("Licht")) CreateLight();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ansicht")) {
            // Demo-Fenster entfernt – Nutzer sagte es hat keine Funktion und ist redundant
            // Stattdessen nur Layout zurücksetzen

            if (ImGui::MenuItem("Layout zurücksetzen")) { mLayoutInitialized = false; }
            ImGui::Separator();

            // Theme selection
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Dunkel", nullptr, mCurrentTheme == EditorTheme::Dark)) {
                    mCurrentTheme = EditorTheme::Dark;
                    EditorStyle::ApplyTheme(EditorTheme::Dark);
                }
                if (ImGui::MenuItem("Hell", nullptr, mCurrentTheme == EditorTheme::Light)) {
                    mCurrentTheme = EditorTheme::Light;
                    EditorStyle::ApplyTheme(EditorTheme::Light);
                }
                if (ImGui::MenuItem("Classic (RPG Maker)", nullptr, mCurrentTheme == EditorTheme::Classic)) {
                    mCurrentTheme = EditorTheme::Classic;
                    EditorStyle::ApplyTheme(EditorTheme::Classic);
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            bool followPlayer = mEngine.IsPlayModeFollowPlayer();
            if (ImGui::MenuItem("Spieler im Spielmodus verfolgen", nullptr, &followPlayer)) {
                mEngine.SetPlayModeFollowPlayer(followPlayer);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Spiel")) {
            bool isPlaying = mEngine.IsPlaying();
            mPlayMode = isPlaying; // sync
            if (ImGui::MenuItem(isPlaying ? "Stop Playtest" : "Playtest starten", "F5")) {
                mEngine.SetPlaying(!isPlaying);
            }
            if (ImGui::MenuItem("Playtest ab Kamera", nullptr, false, !isPlaying)) {
                mEngine.SetPlaying(true);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Playtest: WASD, E sprechen, Esc Pause");
            ImGui::Separator();
            bool follow = mEngine.IsPlayModeFollowPlayer();
            if (ImGui::MenuItem("Kamera folgt Spieler", nullptr, &follow)) {
                mEngine.SetPlayModeFollowPlayer(follow);
            }
            ImGui::EndMenu();
        }

        // Globale Shortcuts
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (mEngine.GetCommandHistory().CanUndo()) {
                mEngine.GetCommandHistory().Undo(mEngine);
            }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            if (mEngine.GetCommandHistory().CanRedo()) {
                mEngine.GetCommandHistory().Redo(mEngine);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput) {
            DeleteSelectedEntity();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            bool isPlaying = mEngine.IsPlaying();
            mEngine.SetPlaying(!isPlaying);
        }

        ImGui::Separator();
        // Spielmodus Status Anzeige
        if (mEngine.IsPlaying()) {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::GetColors().success);
            ImGui::Text("SPIELMODUS");
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
        ImGui::Text("FPS: %d", mEngine.GetFPS());

        ImGui::EndMainMenuBar();
    }
    // Sync play mode
    mPlayMode = mEngine.IsPlaying();
}

void Editor::DrawSceneView() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 size = ImGui::GetContentRegionAvail();
    
    // Stabilize size – round to 2px to reduce framebuffer thrashing / flicker
    size.x = floorf(size.x / 2.0f) * 2.0f;
    size.y = floorf(size.y / 2.0f) * 2.0f;
    if (size.x < 32) size.x = 32;
    if (size.y < 32) size.y = 32;
    
    // Only update if size changed significantly – reduces flicker during resize
    Vec2 newSize(size.x, size.y);
    if (fabsf(newSize.x - mSceneViewSize.x) > 2.0f || fabsf(newSize.y - mSceneViewSize.y) > 2.0f) {
        mSceneViewSize = newSize;
    }
    size.x = mSceneViewSize.x;
    size.y = mSceneViewSize.y;

    unsigned int texId = mEngine.GetSceneTextureID();
    if (texId != 0) {
        ImTextureID img = (ImTextureID)(intptr_t)texId;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        mSceneViewPos = Vec2(pos.x, pos.y);
        mEngine.SetSceneViewRect(mSceneViewPos, mSceneViewSize);

        // Background to avoid flicker
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(30, 30, 35, 255));

        ImGui::Image(img, size, ImVec2(0, 1), ImVec2(1, 0));
        mSceneViewHovered = ImGui::IsItemHovered();
        mSceneViewFocused = ImGui::IsWindowFocused();

        // Gizmo tools bar overlaid on scene view (top-center) - NUR im Editor-Modus, nicht im Playtest
        // Fix: Im Playtest ausgeblendet, damit Buttons nicht durch UI gehen und nicht nutzbar sind
        if (!mEngine.IsPlaying()) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 8.0f, pos.y + 8.0f));
            ImGui::BeginChild("##SceneGizmoBar", ImVec2(size.x - 16.0f, 32.0f), false,
                ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.27f, 0.85f));

            auto modeBtn = [&](const char* label, GizmoMode mode) {
                bool active = (mGizmoMode == mode);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.50f, 0.85f, 0.90f));
                if (ImGui::SmallButton(label)) mGizmoMode = mode;
                if (active) ImGui::PopStyleColor();
                ImGui::SameLine();
            };
            modeBtn("Select (Q)", GizmoMode::None);
            modeBtn("Move (W)", GizmoMode::Translate);
            modeBtn("Rotate (E)", GizmoMode::Rotate);
            modeBtn("Scale (R)", GizmoMode::Scale);
            ImGui::Dummy(ImVec2(8, 0)); ImGui::SameLine();
            if (ImGui::SmallButton(mGizmoSpace == GizmoSpace::Local ? "Local" : "World")) {
                mGizmoSpace = (mGizmoSpace == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Snap", &mGizmoSnap);
            if (mGizmoSnap) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60);
                ImGui::DragFloat("##snapv", &mGizmoSnapValue, 0.05f, 0.05f, 5.0f, "%.2f");
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // Draw interactive gizmo on top of the scene image
        DrawGizmo();

        HandleSceneViewPicking();
        HandleSceneViewCamera();

        // Tile Outline (Fix für "Outline erstellen wenn man auf diesen Boden von x,y rechtsklickt")
        if (mHoverTileValid && mShowTileOutline && !mEngine.IsPlaying() && mGizmoMode == GizmoMode::None) {
            Camera& cam = mEngine.GetRenderer().GetCamera();
            Mat4 view = cam.GetViewMatrix();
            Mat4 proj = cam.GetProjectionMatrix();
            auto worldToScreen = [&](const Vec3& world) -> ImVec2 {
                Vec4 clip = proj * view * Vec4(world, 1.0f);
                if (clip.w <= 0.001f) return ImVec2(-10000, -10000);
                Vec3 ndc(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
                if (ndc.z < -1.0f || ndc.z > 1.0f) return ImVec2(-10000, -10000);
                float x = (ndc.x * 0.5f + 0.5f) * mSceneViewSize.x + mSceneViewPos.x;
                float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * mSceneViewSize.y + mSceneViewPos.y;
                return ImVec2(x, y);
            };
            Map& map = mEngine.GetMap();
            float halfW = map.GetWidth() * 0.5f;
            float halfH = map.GetHeight() * 0.5f;
            float px = (mHoverTileX - halfW);
            float pz = (mHoverTileZ - halfH);
            // 4 Ecken des Tiles
            Vec3 corners[4] = {
                Vec3(px, 0.02f, pz),
                Vec3(px+1.0f, 0.02f, pz),
                Vec3(px+1.0f, 0.02f, pz+1.0f),
                Vec3(px, 0.02f, pz+1.0f)
            };
            ImVec2 scr[4];
            for (int i=0;i<4;++i) scr[i] = worldToScreen(corners[i]);
            bool valid = true;
            for (int i=0;i<4;++i) if (scr[i].x < -9000) valid = false;
            if (valid) {
                // Outline in Gelb für Tile
                draw_list->AddPolyline(scr, 4, IM_COL32(255, 230, 80, 255), ImDrawFlags_Closed, 2.5f);
                // Fill semi transparent
                draw_list->AddConvexPolyFilled(scr, 4, IM_COL32(255, 230, 80, 60));
                // Text mit Koordinaten
                ImVec2 mid((scr[0].x+scr[2].x)*0.5f, (scr[0].y+scr[2].y)*0.5f);
                char buf[32];
                snprintf(buf, sizeof(buf), "(%d,%d)", mHoverTileX, mHoverTileZ);
                draw_list->AddText(mid, IM_COL32(255,255,150,255), buf);
            }
        }

        // Right-click context menu (opens on right click without orbiting if no drag)
        HandleSceneViewContextMenu(pos, size);

        // Overlay info + In-Scene Texte (Fix für "Texte alles vom Fenster sollen kein imgui nutzen sondern richtige Fenster die für Scene Fenster genutzt wird")
        // Jetzt werden Message, HUD etc. direkt ins Scene Fenster gerendert, damit Playtest Fenster sie sieht, nicht nur ImGui
        if (mEngine.IsPlaying()) {
            draw_list->AddText(ImVec2(pos.x + 8, pos.y + 44), IM_COL32(80, 255, 80, 255), "PLAY MODE - Texte im Scene Fenster");
            // MessageWindow als echte Box im Scene Fenster (kein ImGui Fenster mehr nur)
            auto& msg = GameUI::Get().Message();
            if (msg.IsVisible()) {
                // Hintergrund Box unten im Scene Fenster
                float boxH = 110.0f;
                ImVec2 boxMin(pos.x + 10, pos.y + size.y - boxH - 10);
                ImVec2 boxMax(pos.x + size.x - 10, pos.y + size.y - 10);
                draw_list->AddRectFilled(boxMin, boxMax, IM_COL32(20, 25, 35, 220), 6.0f);
                draw_list->AddRect(boxMin, boxMax, IM_COL32(80, 90, 110, 200), 0.0f, 0, 1.5f);
                // Für MessageWindow haben wir keinen direkten Text Zugriff, aber wir zeigen Hinweis, dass ImGui Message gleichzeitig existiert
                draw_list->AddText(ImVec2(boxMin.x + 10, boxMin.y + 8), IM_COL32(220,230,255,255), "Dialog aktiv – siehe unten (E/Enter zum Weiter)");
            }
            // Render ScreenTexts direkt ins Scene Fenster via DrawList (statt nur ImGui Fenster)
            auto& ui = GameUI::Get();
            for (auto& st : ui.GetScreenTexts()) {
                if (st.text.empty()) continue;
                float alpha = 1.0f;
                if (st.fading && st.duration > 0.0f) {
                    float rem = st.duration - st.elapsed;
                    if (rem < 1.0f) alpha = rem;
                }
                if (alpha <= 0.0f) continue;
                ImVec2 tPos;
                if (st.worldSpace) {
                    // Welt-Position zu Screen grob mappen (zentriert)
                    tPos = ImVec2(pos.x + size.x * 0.5f + st.worldPos.x * 20.0f, pos.y + size.y * 0.5f - st.worldPos.z * 20.0f - st.worldPos.y * 10.0f);
                } else {
                    tPos = ImVec2(pos.x + st.screenPos.x * size.x, pos.y + st.screenPos.y * size.y);
                }
                ImU32 col = IM_COL32((int)(st.color.r*255), (int)(st.color.g*255), (int)(st.color.b*255), (int)(alpha*255));
                // Hintergrund
                if (st.withBackground) {
                    ImVec2 txtSize = ImGui::CalcTextSize(st.text.c_str());
                    ImVec2 bgMin(tPos.x - 4, tPos.y - 2);
                    ImVec2 bgMax(tPos.x + txtSize.x + 4, tPos.y + txtSize.y + 2);
                    draw_list->AddRectFilled(bgMin, bgMax, IM_COL32(0,0,0, (int)(alpha*120)), 3.0f);
                }
                draw_list->AddText(tPos, col, st.text.c_str());
            }
        }

        if (mSceneViewHovered && !mEngine.IsPlaying()) {
            ImVec2 hint_pos = ImVec2(pos.x + 8, pos.y + size.y - 22);
            draw_list->AddText(hint_pos, IM_COL32(180, 180, 180, 180),
                "LMB: Paint/Select | RMB: Orbit/Context | MMB: Pan | Wheel: Zoom | Q/W/E/R Gizmo | F: Focus");
            if (mHoverTileValid) {
                ImVec2 tileInfoPos(pos.x + 8, pos.y + size.y - 40);
                char buf[64];
                snprintf(buf, sizeof(buf), "Tile Hover: (%d,%d) - Rechtsklick: Player Start/Event/Boden entfernen", mHoverTileX, mHoverTileZ);
                draw_list->AddText(tileInfoPos, IM_COL32(200,220,255,200), buf);
            }
        }
    } else {
        ImGui::Text("Scene View (%.0f x %.0f)", size.x, size.y);
        ImGui::Text("WASD + Rechtsklick zum Navigieren");
        mSceneViewHovered = false;
        mSceneViewFocused = false;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void Editor::HandleSceneViewPicking() {
    if (!mSceneViewHovered || mEngine.IsPlaying()) {
        mHoverTileValid = false;
        return;
    }
    if (mGizmoActive) return;

    ImVec2 mousePos = ImGui::GetMousePos();
    Vec2 localPos(mousePos.x - mSceneViewPos.x, mousePos.y - mSceneViewPos.y);
    mLastMouseLocal = localPos;
    if (localPos.x < 0 || localPos.y < 0 || localPos.x >= mSceneViewSize.x || localPos.y >= mSceneViewSize.y) {
        mHoverTileValid = false;
        return;
    }

    Camera& cam = mEngine.GetRenderer().GetCamera();
    Ray ray = Raycast::ScreenPointToRay(cam, localPos, mSceneViewSize);

    // Immer Boden-Hit berechnen für Hover-Outline (Fix für "Outline erstellen wenn man auf Boden von x,y rechtsklickt")
    {
        auto hit = Raycast::IntersectPlane(ray, Vec3(0, 1, 0), Vec3(0, 0, 0));
        if (hit.hit) {
            Map& map = mEngine.GetMap();
            float halfW = map.GetWidth() * 0.5f;
            float halfH = map.GetHeight() * 0.5f;
            int x = static_cast<int>(std::floor(hit.point.x + halfW));
            int z = static_cast<int>(std::floor(hit.point.z + halfH));
            if (x >= 0 && x < map.GetWidth() && z >= 0 && z < map.GetHeight()) {
                SetHoverTile(x, z, hit.point);
            } else {
                mHoverTileValid = false;
            }
        } else {
            mHoverTileValid = false;
        }
    }

    bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mSceneViewFocused;
    bool leftDragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && mSceneViewFocused && !ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right) && mSceneViewFocused;

    // Right-click soll nicht direkt malen, sondern Context-Menü – aber Hover bleibt
    if (!leftClicked && !(leftDragging && mGizmoMode == GizmoMode::None)) {
        // Wenn nur Hover (kein Klick), trotzdem Tile-Outline zeigen, aber Entity-Pick optional
        // Entity pick nur bei Linksklick
        return;
    }

    // Entity pick hat Vorrang bei Linksklick
    RaycastHit pick = Raycast::PickEntity(ray, mEngine.GetScene(), 2000.0f);
    if (pick.hit && leftClicked && mGizmoMode != GizmoMode::None) {
        // Wenn GizmoMode == Select (None) wollen wir Tile malen, nicht Entity wählen, falls Tile ausgewählt
        // Aber wenn kein Tile ausgewählt (=Eraser oder -1) und Gizmo Select, dann Entity wählen
        if (mSelectedTile == -1 || mGizmoMode != GizmoMode::None) {
            mSelectedEntity = static_cast<int>(pick.entity);
            RPG_LOG_INFO("Selected entity " + std::to_string(mSelectedEntity));
            return;
        }
    } else if (pick.hit && leftClicked && mGizmoMode != GizmoMode::None) {
        mSelectedEntity = static_cast<int>(pick.entity);
        RPG_LOG_INFO("Selected entity " + std::to_string(mSelectedEntity));
        return;
    }

    // Paint tiles on ground when in select mode – erlaubt auch Radiergummi (-1)
    if (mGizmoMode == GizmoMode::None && (leftClicked || leftDragging)) {
        if (mHoverTileValid) {
            PaintTileAt(mHoverTileX, mHoverTileZ);
        }
    } else if (leftClicked && !pick.hit) {
        if (mGizmoMode != GizmoMode::None) {
            mSelectedEntity = -1;
        }
    }
}

void Editor::HandleSceneViewCamera() {
    if (!mSceneViewHovered || !mSceneViewFocused) return;
    if (mEngine.IsPlaying()) return;
    
    auto& input = mEngine.GetInput();
    Camera& cam = mEngine.GetRenderer().GetCamera();
    float dt = mEngine.GetDeltaTime();
    
    float speed = (input.IsKeyDown(Key::LShift) ? 15.0f : 6.0f) * dt;
    
    // Movement relative to camera direction (WASD)
    if (input.IsKeyDown(Key::W)) cam.SetPosition(cam.GetPosition() + cam.GetForward() * speed);
    if (input.IsKeyDown(Key::S)) cam.SetPosition(cam.GetPosition() - cam.GetForward() * speed);
    if (input.IsKeyDown(Key::A)) cam.SetPosition(cam.GetPosition() - cam.GetRight() * speed);
    if (input.IsKeyDown(Key::D)) cam.SetPosition(cam.GetPosition() + cam.GetRight() * speed);
    if (input.IsKeyDown(Key::Q)) cam.SetPosition(cam.GetPosition() + Vec3(0, 1, 0) * speed);  // Up
    if (input.IsKeyDown(Key::E)) cam.SetPosition(cam.GetPosition() - Vec3(0, 1, 0) * speed);  // Down

    // Orbit / Look around: Right mouse drag
    if (input.IsMouseDown(MouseButton::Right)) {
        Vec2 delta = input.GetMouseDelta();
        Vec3 rot = cam.GetRotation();
        rot.y -= delta.x * 0.3f;   // Yaw (horizontal)
        rot.x -= delta.y * 0.3f;   // Pitch (vertical)
        rot.x = glm::clamp(rot.x, -89.0f, 89.0f);
        cam.SetRotation(rot);
    }

    // Mouse wheel: Zoom (dolly)
    if (input.GetMouseWheel() != 0.0f) {
        cam.SetPosition(cam.GetPosition() + cam.GetForward() * input.GetMouseWheel() * 3.0f);
    }
    
    // Middle mouse: Pan
    if (input.IsMouseDown(MouseButton::Middle)) {
        Vec2 delta = input.GetMouseDelta();
        Vec3 right = cam.GetRight();
        Vec3 up = cam.GetUp();
        cam.SetPosition(cam.GetPosition() - right * delta.x * 0.01f * speed * 10.0f + up * delta.y * 0.01f * speed * 10.0f);
    }
    
    // Focus on selected entity: F key
    if (input.IsKeyPressed(Key::F) && mSelectedEntity >= 0) {
        auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
        if (transform) {
            Vec3 target = transform->transform.position;
            cam.SetPosition(target + Vec3(0, 3, 5));
            cam.SetRotation(Vec3(-30, 0, 0));
        }
    }
}

void Editor::HandleSceneViewContextMenu(const ImVec2& viewPos, const ImVec2& viewSize) {
    if (!mSceneViewHovered) return;

    // Open on right-click release only if almost no drag (so orbit still works)
    static ImVec2 rmbDownPos(0, 0);
    static bool rmbWasDown = false;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && mSceneViewFocused) {
        rmbDownPos = ImGui::GetMousePos();
        rmbWasDown = true;
    }
    if (rmbWasDown && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        ImVec2 up = ImGui::GetMousePos();
        float dx = up.x - rmbDownPos.x, dy = up.y - rmbDownPos.y;
        if ((dx*dx + dy*dy) < 16.0f) { // ~4px
            ImGui::OpenPopup("SceneViewContextMenu");
        }
        rmbWasDown = false;
    }

    if (ImGui::BeginPopup("SceneViewContextMenu")) {
        ImGui::TextDisabled("Scene");
        ImGui::Separator();

        // Zeige Hover Tile Info
        if (mHoverTileValid) {
            ImGui::Text("Tile: (%d, %d)", mHoverTileX, mHoverTileZ);
            ImGui::Separator();
        }

        // --- NEW: Player Start und Event Creation (Fix für "PlayerPosition auf dieser Map_ID Platzieren") ---
        if (ImGui::BeginMenu("Spiel-Logik")) {
            if (mHoverTileValid) {
                if (ImGui::MenuItem("Player Start hier setzen (Map_ID)")) {
                    SetPlayerStartAt(mHoverTileX, mHoverTileZ);
                }
                if (ImGui::MenuItem("Event hier erstellen (Standard)")) {
                    CreateEventAt(mHoverTileX, mHoverTileZ, false);
                }
                if (ImGui::MenuItem("NPC Event hier erstellen")) {
                    CreateEventAt(mHoverTileX, mHoverTileZ, true);
                }
                if (ImGui::MenuItem("Boden entfernen (x,y)")) {
                    EraseTileAt(mHoverTileX, mHoverTileZ);
                }
            } else {
                ImGui::TextDisabled("Kein Tile unter Maus – auf Boden zielen");
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Reset Camera")) {
            Camera& cam = mEngine.GetRenderer().GetCamera();
            cam.SetPosition(Vec3(0, 10, 10));
            cam.SetRotation(Vec3(-45, 0, 0));
        }
        if (ImGui::MenuItem("Focus Selection", "F", false, mSelectedEntity >= 0)) {
            if (mSelectedEntity >= 0) {
                auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
                if (transform) {
                    Camera& cam = mEngine.GetRenderer().GetCamera();
                    Vec3 target = transform->transform.position;
                    cam.SetPosition(target + Vec3(0, 3, 5));
                    cam.SetRotation(Vec3(-30, 0, 0));
                }
            }
        }
        if (ImGui::MenuItem("Frame All")) {
            Camera& cam = mEngine.GetRenderer().GetCamera();
            cam.SetPosition(Vec3(0, 12, 16));
            cam.SetRotation(Vec3(-35, 0, 0));
        }

        ImGui::Separator();
        ImGui::TextDisabled("Create");

        ImVec2 mousePos = ImGui::GetMousePos();
        Vec2 localPos(mousePos.x - viewPos.x, mousePos.y - viewPos.y);
        Camera& cam = mEngine.GetRenderer().GetCamera();
        Ray ray = Raycast::ScreenPointToRay(cam, localPos, mSceneViewSize);
        auto hit = Raycast::IntersectPlane(ray, Vec3(0, 1, 0), Vec3(0, 0, 0));
        Vec3 spawn = hit.hit ? hit.point : Vec3(0, 0, 0);

        if (ImGui::MenuItem("Cube")) {
            CreateCube();
            if (mSelectedEntity >= 0) {
                if (auto* t = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity)))
                    t->transform.position = spawn + Vec3(0, 0.5f, 0);
            }
        }
        if (ImGui::MenuItem("Plane")) {
            CreatePlane();
            if (mSelectedEntity >= 0) {
                if (auto* t = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity)))
                    t->transform.position = spawn;
            }
        }
        if (ImGui::MenuItem("Light")) {
            CreateLight();
            if (mSelectedEntity >= 0) {
                if (auto* t = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity)))
                    t->transform.position = spawn + Vec3(0, 3.0f, 0);
            }
        }
        if (ImGui::MenuItem("Particle Emitter")) {
            EntityID id = mEngine.GetScene().CreateEntity("Particles");
            auto* tr = mEngine.GetScene().AddComponent<TransformComponent>(id);
            tr->transform.position = spawn + Vec3(0, 0.5f, 0);
            auto* pe = mEngine.GetScene().AddComponent<ParticleEmitterComponent>(id);
            pe->emitter = std::make_unique<ParticleEmitter>();
            pe->emitter->ApplyPreset("fire");
            pe->autoEmit = true;
            pe->emitCount = pe->emitter->defaultCount;
            pe->emitDirection = pe->emitter->defaultDirection;
            pe->emitSpread = pe->emitter->defaultSpread;
            pe->emitSpeed = pe->emitter->defaultSpeed;
            pe->emitLife = pe->emitter->defaultLife;
            pe->emitColor = pe->emitter->defaultStartColor;
            mSelectedEntity = static_cast<int>(id);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Edit");
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, mSelectedEntity >= 0)) {
            if (mSelectedEntity >= 0) {
                EntityID srcId = static_cast<EntityID>(mSelectedEntity);
                auto* st = mEngine.GetScene().GetComponent<TransformComponent>(srcId);
                EntityID id = mEngine.GetScene().CreateEntity(mEngine.GetScene().GetEntityName(srcId) + " Copy");
                auto* t = mEngine.GetScene().AddComponent<TransformComponent>(id);
                if (st) {
                    t->transform = st->transform;
                    t->transform.position.x += 1.0f;
                }
                if (auto* sm = mEngine.GetScene().GetComponent<ModelRendererComponent>(srcId)) {
                    auto* m = mEngine.GetScene().AddComponent<ModelRendererComponent>(id);
                    m->model = sm->model;
                    m->texture = sm->texture;
                }
                if (auto* mat = mEngine.GetScene().GetComponent<MaterialComponent>(srcId)) {
                    auto* m = mEngine.GetScene().AddComponent<MaterialComponent>(id);
                    m->material = mat->material;
                }
                mSelectedEntity = static_cast<int>(id);
            }
        }
        if (ImGui::MenuItem("Delete", "Del", false, mSelectedEntity >= 0)) {
            DeleteSelectedEntity();
        }
        if (ImGui::BeginMenu("Gizmo Mode")) {
            if (ImGui::MenuItem("Select", "Q", mGizmoMode == GizmoMode::None)) mGizmoMode = GizmoMode::None;
            if (ImGui::MenuItem("Move", "W", mGizmoMode == GizmoMode::Translate)) mGizmoMode = GizmoMode::Translate;
            if (ImGui::MenuItem("Rotate", "E", mGizmoMode == GizmoMode::Rotate)) mGizmoMode = GizmoMode::Rotate;
            if (ImGui::MenuItem("Scale", "R", mGizmoMode == GizmoMode::Scale)) mGizmoMode = GizmoMode::Scale;
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::TextDisabled("View");
        if (ImGui::MenuItem("Toggle Grid", "G")) {
            mEngine.ToggleGrid();
        }
        if (ImGui::MenuItem("Toggle Wireframe")) {
            mEngine.GetRenderer().EnableWireframe(!mEngine.GetRenderer().IsWireframeEnabled());
        }
        if (ImGui::MenuItem("Snap", nullptr, mGizmoSnap)) {
            mGizmoSnap = !mGizmoSnap;
        }

        ImGui::EndPopup();
    }
}

void Editor::PaintTileAt(int x, int z) {
    Map& map = mEngine.GetMap();
    mPaintX = x;
    mPaintZ = z;
    int oldTile = map.GetTile(mSelectedLayer, x, z);
    // mSelectedTile == -1 bedeutet Radiergummi (Boden entfernen) – FIX für fehlende Funktion
    int newTile = mSelectedTile;
    auto cmd = std::make_shared<SetTileCommand>(mSelectedLayer, x, z, oldTile, newTile);
    mEngine.GetCommandHistory().Execute(mEngine, cmd);
    if (newTile == -1) {
        RPG_LOG_INFO("Erased tile at (" + std::to_string(x) + ", " + std::to_string(z) + ")");
    } else {
        RPG_LOG_INFO("Painted tile " + std::to_string(newTile) + " at (" + std::to_string(x) + ", " + std::to_string(z) + ")");
    }
}

void Editor::EraseTileAt(int x, int z) {
    Map& map = mEngine.GetMap();
    int oldTile = map.GetTile(mSelectedLayer, x, z);
    if (oldTile == -1) return;
    auto cmd = std::make_shared<SetTileCommand>(mSelectedLayer, x, z, oldTile, -1);
    mEngine.GetCommandHistory().Execute(mEngine, cmd);
    RPG_LOG_INFO("Erased tile at (" + std::to_string(x) + ", " + std::to_string(z) + ")");
}

void Editor::ClearCurrentMapLayer() {
    Map& map = mEngine.GetMap();
    int w = map.GetWidth();
    int h = map.GetHeight();
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            int oldTile = map.GetTile(mSelectedLayer, x, z);
            if (oldTile != -1) {
                auto cmd = std::make_shared<SetTileCommand>(mSelectedLayer, x, z, oldTile, -1);
                mEngine.GetCommandHistory().Execute(mEngine, cmd);
            }
        }
    }
    RPG_LOG_INFO("Cleared layer " + std::to_string(mSelectedLayer));
}

void Editor::FillCurrentMapLayer(int tileId) {
    Map& map = mEngine.GetMap();
    int w = map.GetWidth();
    int h = map.GetHeight();
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            int oldTile = map.GetTile(mSelectedLayer, x, z);
            if (oldTile != tileId) {
                auto cmd = std::make_shared<SetTileCommand>(mSelectedLayer, x, z, oldTile, tileId);
                mEngine.GetCommandHistory().Execute(mEngine, cmd);
            }
        }
    }
    RPG_LOG_INFO("Filled layer " + std::to_string(mSelectedLayer) + " with tile " + std::to_string(tileId));
}

void Editor::SetHoverTile(int x, int z, const Vec3& worldPos) {
    mHoverTileX = x;
    mHoverTileZ = z;
    mHoverTileValid = true;
    mHoverWorldPos = worldPos;
}

void Editor::SetPlayerStartAt(int x, int z) {
    auto& sys = Database::Get().System();
    // Map Id aus aktueller Auswahl, sonst 1
    int mapId = 1;
    if (mSelectedMapIndex >= 0 && mSelectedMapIndex < (int)Database::Get().MapInfos().size()) {
        mapId = Database::Get().MapInfos()[mSelectedMapIndex].id;
    }
    sys.startMapId = mapId;
    sys.startX = x;
    sys.startY = z;
    RPG_LOG_INFO("Player Start gesetzt: Map " + std::to_string(mapId) + " X=" + std::to_string(x) + " Y=" + std::to_string(z));
}

void Editor::CreateEventAt(int x, int z, bool asNPC) {
    auto& eventSystem = EventSystem::Get();
    auto& events = eventSystem.GetEvents();
    MapEvent newEvent;
    newEvent.id = events.empty() ? 1 : events.back().id + 1;
    newEvent.name = asNPC ? ("NPC_" + std::to_string(newEvent.id)) : ("EV" + std::to_string(newEvent.id));
    newEvent.x = x;
    newEvent.y = z;
    newEvent.z = 0;
    EventPage page;
    page.id = 1;
    page.trigger = EventTrigger::ActionButton;
    page.graphicName = asNPC ? "npc" : "";
    page.graphicIndex = 0;
    newEvent.pages.push_back(page);
    newEvent.currentPage = 0;
    newEvent.enabled = true;
    eventSystem.AddEvent(newEvent);
    mSelectedEventId = newEvent.id;
    mSelectedEventPage = 0;
    RPG_LOG_INFO("Event erstellt bei (" + std::to_string(x) + "," + std::to_string(z) + ") Name=" + newEvent.name);
}

void Editor::DrawHierarchy() {
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Hierarchy");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("PLAYTEST AKTIV");
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Editor deaktiviert - F5 zum Stoppen");
        ImGui::End();
        return;
    }
    ImGui::Begin("Hierarchy");
    Scene& scene = mEngine.GetScene();

    if (ImGui::Button((std::string(Icons::PLUS) + " Entity").c_str())) {
        CreateCube();
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icons::TRASH) + " Del").c_str()) && mSelectedEntity >= 0) {
        DeleteSelectedEntity();
    }
    ImGui::Separator();

    for (EntityID id : scene.GetEntities()) {
        bool selected = (mSelectedEntity == static_cast<int>(id));
        // Icon by components
        const char* icon = Icons::CUBE;
        if (scene.GetComponent<LightComponent>(id)) icon = Icons::LIGHTBULB;
        else if (scene.GetComponent<ParticleEmitterComponent>(id)) icon = Icons::MAGIC;
        else if (scene.GetComponent<CameraComponent>(id)) icon = Icons::CAMERA;
        else if (scene.GetComponent<ModelRendererComponent>(id)) icon = Icons::CUBE;

        std::string label = std::string(icon) + "  " + scene.GetEntityName(id);
        ImGui::PushID(static_cast<int>(id));
        if (ImGui::Selectable(label.c_str(), selected)) {
            mSelectedEntity = static_cast<int>(id);
        }
        if (ImGui::BeginPopupContextItem("HierarchyCtx")) {
            mSelectedEntity = static_cast<int>(id);
            if (ImGui::MenuItem("Focus", "F")) {
                if (auto* t = scene.GetComponent<TransformComponent>(id)) {
                    Camera& cam = mEngine.GetRenderer().GetCamera();
                    cam.SetPosition(t->transform.position + Vec3(0, 3, 5));
                    cam.SetRotation(Vec3(-30, 0, 0));
                }
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                // reuse shortcut logic via synthetic key path: call create copy
                EntityID srcId = id;
                auto* st = scene.GetComponent<TransformComponent>(srcId);
                EntityID nid = scene.CreateEntity(scene.GetEntityName(srcId) + " Copy");
                auto* nt = scene.AddComponent<TransformComponent>(nid);
                if (st) { nt->transform = st->transform; nt->transform.position.x += 1.0f; }
                if (auto* sm = scene.GetComponent<ModelRendererComponent>(srcId)) {
                    auto* mcomp = scene.AddComponent<ModelRendererComponent>(nid);
                    mcomp->model = sm->model; mcomp->texture = sm->texture;
                }
                if (auto* mat = scene.GetComponent<MaterialComponent>(srcId)) {
                    auto* mcomp = scene.AddComponent<MaterialComponent>(nid);
                    mcomp->material = mat->material;
                }
                mSelectedEntity = static_cast<int>(nid);
            }
            if (ImGui::MenuItem("Delete", "Del")) {
                DeleteSelectedEntity();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Add Light")) {
                if (!scene.GetComponent<LightComponent>(id)) scene.AddComponent<LightComponent>(id);
            }
            if (ImGui::MenuItem("Add Particles")) {
                if (!scene.GetComponent<ParticleEmitterComponent>(id)) {
                    auto* pe = scene.AddComponent<ParticleEmitterComponent>(id);
                    pe->emitter = std::make_unique<ParticleEmitter>();
                    pe->emitter->ApplyPreset("fire");
                    pe->autoEmit = true;
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // empty area context
    if (ImGui::BeginPopupContextWindow("HierarchyEmpty", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("Create Cube")) CreateCube();
        if (ImGui::MenuItem("Create Plane")) CreatePlane();
        if (ImGui::MenuItem("Create Light")) CreateLight();
        ImGui::EndPopup();
    }

    ImGui::End();
}


void Editor::DrawInspector() {
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Inspector");
        ImGui::TextDisabled("Deaktiviert während Playtest");
        ImGui::End();
        return;
    }
    ImGui::Begin("Inspector");
    if (mSelectedEntity >= 0) {
        EntityID id = static_cast<EntityID>(mSelectedEntity);
        ImGui::Text("Entity: %s", mEngine.GetScene().GetEntityName(id).c_str());

        auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(id);
        if (transform) {
            if (ImGui::TreeNode("Transform")) {
                Vec3 oldPos = transform->transform.position;
                ImGui::DragFloat3("Position", &transform->transform.position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &transform->transform.rotation.x, 0.5f);
                ImGui::DragFloat3("Scale", &transform->transform.scale.x, 0.05f);
                if (oldPos != transform->transform.position) {
                    // Note: continuous drag would spam history; simplified
                }
                ImGui::TreePop();
            }
        }

        auto* material = mEngine.GetScene().GetComponent<MaterialComponent>(id);
        if (material) {
            if (ImGui::TreeNode("Material")) {
                ImGui::ColorEdit4("Diffuse", &material->material.diffuse.x);
                ImGui::ColorEdit3("Emissive", &material->material.emissive.x);
                ImGui::SliderFloat("Metallic", &material->material.metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &material->material.roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Alpha", &material->material.alpha, 0.0f, 1.0f);
                ImGui::Checkbox("Transparent", &material->material.transparent);
                ImGui::Checkbox("Wireframe", &material->material.wireframe);
                ImGui::TreePop();
            }
        }

        auto* light = mEngine.GetScene().GetComponent<LightComponent>(id);
        if (light) {
            if (ImGui::TreeNode("Light")) {
                ImGui::ColorEdit4("Color", &light->color.x);
                ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 10.0f);
                ImGui::DragFloat("Range", &light->range, 0.1f, 0.0f, 100.0f);
                ImGui::TreePop();
            }
        }

        auto* emitter = mEngine.GetScene().GetComponent<ParticleEmitterComponent>(id);
        if (emitter) {
            if (ImGui::TreeNode("Particle Emitter")) {
                if (!emitter->emitter) emitter->emitter = std::make_unique<ParticleEmitter>();
                ImGui::Checkbox("Auto Emit", &emitter->autoEmit);
                ImGui::SliderInt("Emit Count", &emitter->emitCount, 1, 50);
                ImGui::SliderFloat("Emit Rate", &emitter->emitRate, 0.01f, 2.0f);
                ImGui::DragFloat3("Direction", &emitter->emitDirection.x, 0.05f);
                ImGui::SliderFloat("Spread", &emitter->emitSpread, 0.0f, 2.0f);
                ImGui::SliderFloat("Speed", &emitter->emitSpeed, 0.0f, 10.0f);
                ImGui::SliderFloat("Life", &emitter->emitLife, 0.1f, 5.0f);
                ImGui::ColorEdit4("Color", &emitter->emitColor.x);
                ImGui::Text("Presets");
                if (ImGui::SmallButton("Fire")) emitter->emitter->ApplyPreset("fire");
                ImGui::SameLine();
                if (ImGui::SmallButton("Smoke")) emitter->emitter->ApplyPreset("smoke");
                ImGui::SameLine();
                if (ImGui::SmallButton("Spark")) emitter->emitter->ApplyPreset("spark");
                ImGui::SameLine();
                if (ImGui::SmallButton("Magic")) emitter->emitter->ApplyPreset("magic");
                ImGui::SameLine();
                if (ImGui::SmallButton("Heal")) emitter->emitter->ApplyPreset("heal");
                ImGui::Text("Alive: %d / %d", emitter->emitter->GetAliveCount(), emitter->emitter->GetMaxParticles());
                if (ImGui::Button("Burst")) {
                    auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(id);
                    Vec3 origin = transform ? transform->transform.position : Vec3(0.0f);
                    emitter->emitter->Emit(emitter->emitCount, origin, emitter->emitDirection,
                        emitter->emitSpread, emitter->emitSpeed, emitter->emitLife, emitter->emitColor);
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear")) emitter->emitter->Clear();
                ImGui::TreePop();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Delete Entity", ImVec2(-1, 0))) {
            DeleteSelectedEntity();
        }

        static char prefabName[128] = "";
        ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
        ImGui::Separator();
        ImGui::Text("Add Component");
        if (ImGui::Button("Add Material")) {
            if (!mEngine.GetScene().GetComponent<MaterialComponent>(id)) {
                mEngine.GetScene().AddComponent<MaterialComponent>(id);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Light")) {
            if (!mEngine.GetScene().GetComponent<LightComponent>(id)) {
                mEngine.GetScene().AddComponent<LightComponent>(id);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Particles")) {
            if (!mEngine.GetScene().GetComponent<ParticleEmitterComponent>(id)) {
                auto* pe = mEngine.GetScene().AddComponent<ParticleEmitterComponent>(id);
                pe->emitter = std::make_unique<ParticleEmitter>();
            }
        }

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
                    auto* material = scene.GetComponent<MaterialComponent>(id);
                    if (material) data.color = material->material.diffuse;
                    else data.color = Color(1.0f);
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
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Projekt");
        ImGui::TextDisabled("Deaktiviert während Playtest");
        ImGui::End();
        return;
    }
    ImGui::Begin("Projekt");
    ImGui::Text("Projekt: %s", mEngine.GetProject().GetInfo().name.c_str());
    ImGui::Text("Pfad: %s", mEngine.GetProject().GetProjectPath().c_str());
    ImGui::Separator();

    ImGui::Text("Assets");
    std::string basePath = mEngine.GetProject().GetProjectPath();
    if (basePath.empty()) basePath = ".";

    auto drawAsset = [&](const fs::path& path) {
        std::string name = path.filename().string();
        std::string ext = path.extension().string();
        bool isAudio = (ext == ".wav" || ext == ".ogg" || ext == ".mp3");
        bool isModel = (ext == ".obj");
        bool isImage = (ext == ".png" || ext == ".jpg");

        ImGui::BulletText("%s", name.c_str());
        if (ImGui::BeginPopupContextItem(name.c_str())) {
            if (isModel && ImGui::MenuItem("Als Entity importieren")) {
                auto cmd = std::make_shared<CreateEntityCommand>(name);
                mEngine.GetCommandHistory().Execute(mEngine, cmd);
                EntityID id = cmd->GetEntityID();
                if (id != INVALID_ENTITY) {
                    auto* transform = mEngine.GetScene().AddComponent<TransformComponent>(id);
                    transform->transform.position = Vec3(0, 0.5f, 0);
                    auto* model = mEngine.GetScene().AddComponent<ModelRendererComponent>(id);
                    model->model = std::make_shared<Model>();
                    // PAKET 46: auch .anim-Manifeste (Keyframe-Morph) importieren
                    model->model->LoadAnyModelFile(path.string());
                    mSelectedEntity = static_cast<int>(id);
                }
            }
            if (isImage && ImGui::MenuItem("Als Tileset festlegen")) {
                auto tileset = std::make_shared<Tileset>();
                tileset->Load(path.string(), 32, 32);
                mEngine.GetMap().SetTileset(tileset);
            }
            if (isAudio && ImGui::MenuItem("Audio-Vorschau")) {
                if (mAudioPreview) mAudioPreview->LoadAndPlay(path.string());
            }
            ImGui::EndPopup();
        }
    };

    try {
        if (fs::exists(basePath + "/assets")) {
            for (const auto& entry : fs::directory_iterator(basePath + "/assets")) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    if (ImGui::TreeNode(name.c_str())) {
                        for (const auto& sub : fs::directory_iterator(entry.path())) {
                            drawAsset(sub.path());
                        }
                        ImGui::TreePop();
                    }
                } else {
                    drawAsset(entry.path());
                }
            }
        } else {
            ImGui::Text("Kein Assets-Ordner gefunden.");
        }
    } catch (...) {
        ImGui::Text("Projektordner konnte nicht gelesen werden.");
    }

    ImGui::End();
}


void Editor::DrawMapEditor() {
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Karten-Editor");
        ImGui::TextDisabled("Deaktiviert während Playtest - Karte kann nur im Editor bearbeitet werden");
        ImGui::End();
        return;
    }
    ImGui::Begin("Karten-Editor");
    
    auto& database = Database::Get();
    auto& mapInfos = database.MapInfos();
    auto& map = mEngine.GetMap();
    
    // Toolbar
    if (ImGui::Button("Neue Karte")) {
        MapInfo newMap;
        newMap.id = mapInfos.empty() ? 1 : mapInfos.back().id + 1;
        newMap.name = "Karte " + std::to_string(newMap.id);
        newMap.width = 20;
        newMap.height = 15;
        newMap.tilesetId = 1;
        newMap.bgmAutoPlay = true;
        newMap.bgsAutoPlay = true;
        newMap.scrollType = 0;
        newMap.encounterStep = 30;
        newMap.backgroundColor = Color(0, 0, 0, 1);
        newMap.fogColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
        mapInfos.push_back(newMap);
        mSelectedMapIndex = static_cast<int>(mapInfos.size()) - 1;
        LoadSelectedMap();
    }
    ImGui::SameLine();
    if (ImGui::Button("Karte löschen") && mSelectedMapIndex >= 0 && mSelectedMapIndex < static_cast<int>(mapInfos.size())) {
        if (mapInfos.size() > 1) {
            mapInfos.erase(mapInfos.begin() + mSelectedMapIndex);
            mSelectedMapIndex = std::max(0, mSelectedMapIndex - 1);
            LoadSelectedMap();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Nach oben") && mSelectedMapIndex > 0) {
        std::swap(mapInfos[mSelectedMapIndex], mapInfos[mSelectedMapIndex - 1]);
        mSelectedMapIndex--;
        // Order aktualisieren
        for (size_t i = 0; i < mapInfos.size(); ++i) mapInfos[i].order = static_cast<int>(i);
    }
    ImGui::SameLine();
    if (ImGui::Button("Nach unten") && mSelectedMapIndex >= 0 && mSelectedMapIndex < static_cast<int>(mapInfos.size()) - 1) {
        std::swap(mapInfos[mSelectedMapIndex], mapInfos[mSelectedMapIndex + 1]);
        mSelectedMapIndex++;
        for (size_t i = 0; i < mapInfos.size(); ++i) mapInfos[i].order = static_cast<int>(i);
    }
    
    ImGui::Separator();
    
    // Karte laden/speichern
    if (ImGui::Button("Karte speichern")) {
        // Save full package for current map id
        if (mSelectedMapIndex >= 0 && mSelectedMapIndex < static_cast<int>(mapInfos.size())) {
            int mapId = mapInfos[mSelectedMapIndex].id;
            std::string scenePath = mEngine.GetProject().GetProjectPath() + "/maps/map" + std::to_string(mapId) + "_scene.json";
            mEngine.SaveScene(scenePath);
            mEngine.GetMap().Save(mEngine.GetProject().GetMapPath(mapId));
        }
        SaveMap();
        database.Save(mEngine.GetProject().GetProjectPath());
    }
    ImGui::SameLine();
    if (ImGui::Button("Karte laden") && mSelectedMapIndex >= 0) {
        LoadSelectedMap();
    }
    
    ImGui::Separator();
    
    // === LINKS: Karten-Liste (wie RPG Maker) ===
    ImGui::BeginChild("KartenListe", ImVec2(250, 0), true);
    ImGui::Text("Karten (MapInfos)");
    ImGui::Separator();
    
    for (size_t i = 0; i < mapInfos.size(); ++i) {
        auto& info = mapInfos[i];
        bool selected = (static_cast<int>(i) == mSelectedMapIndex);
        std::string label = std::to_string(info.id) + ": " + info.name;
        if (ImGui::Selectable(label.c_str(), selected)) {
            mSelectedMapIndex = static_cast<int>(i);
            LoadSelectedMap();
        }
    }
    
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // === RECHTS: Karten-Eigenschaften + Tile-Editor ===
    ImGui::BeginChild("KartenEigenschaften", ImVec2(0, 0), true);
    
    if (mSelectedMapIndex >= 0 && mSelectedMapIndex < static_cast<int>(mapInfos.size())) {
        auto& currentMap = mapInfos[mSelectedMapIndex];
        
        // Tabs für verschiedene Eigenschaften
        if (ImGui::BeginTabBar("KartenTabs")) {
            
            // ==== HAUPT-TAB ====
            if (ImGui::BeginTabItem("Haupt")) {
                ImGui::Text("Karten-ID: %d", currentMap.id);
                ImGui::Separator();
                
                static char mapName[128];
                if (mapName[0] == 0) strcpy(mapName, currentMap.name.c_str());
                if (ImGui::InputText("Name", mapName, sizeof(mapName))) {
                    currentMap.name = mapName;
                }
                
                ImGui::Separator();
                ImGui::Text("Größe");
                ImGui::DragInt("Breite", &currentMap.width, 1, 1, 500);
                ImGui::DragInt("Höhe", &currentMap.height, 1, 1, 500);
                
                if (ImGui::Button("Karte vergrößern")) {
                    ResizeCurrentMap(currentMap.width, currentMap.height);
                }
                
                ImGui::Separator();
                ImGui::Text("Tileset");
                int tilesetId = currentMap.tilesetId;
                if (ImGui::DragInt("Tileset ID", &tilesetId, 1, 1, 999)) {
                    currentMap.tilesetId = tilesetId;
                    LoadTilesetForMap(tilesetId);
                }
                
                ImGui::EndTabItem();
            }
            
            // ==== TILE-PALETTE ====
            if (ImGui::BeginTabItem((std::string(Icons::PAINT_BRUSH) + " Tile-Palette").c_str())) {
                auto tileset = mEngine.GetMap().GetTileset();
                if (tileset) {
                    ImGui::Text("Tileset: %dx%d tiles (%dx%d)",
                        tileset->GetColumns(), tileset->GetRows(),
                        tileset->GetTileWidth(), tileset->GetTileHeight());

                    ImGui::Separator();
                    if (mSelectedTile == -1) {
                        ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Selected Tile: RADIERGUMMI (Boden entfernen)");
                    } else {
                        ImGui::Text("Selected Tile: %d", mSelectedTile);
                    }
                    if (mHoverTileValid) {
                        ImGui::Text("Hover Tile: (%d, %d) World (%.1f, %.1f)", mHoverTileX, mHoverTileZ, mHoverWorldPos.x, mHoverWorldPos.z);
                    }
                    ImGui::TextDisabled("In Scene: Gizmo=Select (Q), LMB malen, RMB Context (Player Start, Event, Boden entfernen)");
                    ImGui::Separator();

                    // Eraser + Clear/Fill Buttons (Fix für "Boden von x,y auch entfernen kann")
                    if (ImGui::Button(mSelectedTile==-1 ? ">> Radiergummi AKTIV <<" : "Radiergummi (Boden entfernen)")) {
                        mSelectedTile = -1;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Ebene leeren")) {
                        ClearCurrentMapLayer();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Ebene füllen")) {
                        if (mSelectedTile >= 0) FillCurrentMapLayer(mSelectedTile);
                        else ImGui::OpenPopup("FillWarning");
                    }
                    if (ImGui::BeginPopup("FillWarning")) {
                        ImGui::Text("Bitte erst ein Tile wählen zum Füllen, nicht Radiergummi.");
                        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                    ImGui::Separator();

                    // Tile grid
                    int columns = tileset->GetColumns();
                    int rows = tileset->GetRows();
                    int tileCount = columns * rows;

                    float tileSize = 32.0f * mTileScale;
                    float spacing = 2.0f;
                    int colsPerRow = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (tileSize + spacing)));

                    if (ImGui::BeginChild("TileGrid", ImVec2(0, 0), true)) {
                        ImTextureID texId = (ImTextureID)(intptr_t)(tileset->GetTexture() ? tileset->GetTexture()->GetID() : 0);

                        for (int i = 0; i < tileCount; ++i) {
                            int col = i % colsPerRow;

                            if (col > 0) ImGui::SameLine();

                            // GetTileUV returns Vec4(u0, v0, u1, v1)
                            Vec4 uv = tileset->GetTileUV(i);
                            ImVec2 uv0(uv.x, uv.y);
                            ImVec2 uv1(uv.z, uv.w);

                            bool selected = (mSelectedTile == i);
                            ImVec4 tint = selected ? ImVec4(1.0f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                            ImGui::PushID(i);
                            // ImGui 1.91+ ImageButton: (str_id, tex, size, uv0, uv1, bg_col, tint_col)
                            if (ImGui::ImageButton("##tile", texId, ImVec2(tileSize, tileSize),
                                uv0, uv1, ImVec4(0, 0, 0, 0), tint)) {
                                mSelectedTile = i;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Tile %d", i);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndChild();
                    }
                } else {
                    ImGui::Text("Kein Tileset geladen. Wählen Sie eine Tileset-ID im Haupt-Tab.");
                }
                ImGui::EndTabItem();
            }
            
            // ==== MUSIK & HINTERGRUND ====
            if (ImGui::BeginTabItem("Musik & Hintergrund")) {
                ImGui::Text("Hintergrundmusik (BGM)");
                static char bgmName[256];
                if (bgmName[0] == 0) strcpy(bgmName, currentMap.bgmName.c_str());
                if (ImGui::InputText("BGM Datei", bgmName, sizeof(bgmName))) {
                    currentMap.bgmName = bgmName;
                }
                ImGui::Checkbox("Autoplay BGM", &currentMap.bgmAutoPlay);
                if (ImGui::Button("BGM testen") && !currentMap.bgmName.empty()) {
                    mEngine.GetAudio().PlayBGM(currentMap.bgmName, true);
                }
                ImGui::SameLine();
                if (ImGui::Button("BGM stoppen")) {
                    mEngine.GetAudio().FadeOutBGM(0.5f);
                }
                
                ImGui::Separator();
                ImGui::Text("Hintergrundgeräusche (BGS)");
                static char bgsName[256];
                if (bgsName[0] == 0) strcpy(bgsName, currentMap.bgsName.c_str());
                if (ImGui::InputText("BGS Datei", bgsName, sizeof(bgsName))) {
                    currentMap.bgsName = bgsName;
                }
                ImGui::Checkbox("Autoplay BGS", &currentMap.bgsAutoPlay);
                
                ImGui::Separator();
                ImGui::Text("Hintergrund-Typ");
                const char* bgTypes[] = { "Parallaxe", "Farbe" };
                int bgType = currentMap.backgroundType - 1;
                if (ImGui::Combo("Hintergrund", &bgType, bgTypes, 2)) {
                    currentMap.backgroundType = bgType + 1;
                }
                
                if (currentMap.backgroundType == 1) {
                    ImGui::Text("Parallaxe");
                    static char parallaxName[256];
                    if (parallaxName[0] == 0) strcpy(parallaxName, currentMap.parallaxName.c_str());
                    if (ImGui::InputText("Datei", parallaxName, sizeof(parallaxName))) {
                        currentMap.parallaxName = parallaxName;
                    }
                    ImGui::Checkbox("Zeigen", &currentMap.parallaxShow);
                    ImGui::DragInt("Loop X", &currentMap.parallaxLoopX);
                    ImGui::DragInt("Loop Y", &currentMap.parallaxLoopY);
                    ImGui::DragInt("Scroll X", &currentMap.parallaxSx);
                    ImGui::DragInt("Scroll Y", &currentMap.parallaxSy);
                } else {
                    ImGui::ColorEdit4("Hintergrundfarbe", &currentMap.backgroundColor.x);
                }
                
                ImGui::EndTabItem();
            }
            
            // ==== NEBEL (FOG) ====
            if (ImGui::BeginTabItem("Nebel")) {
                ImGui::Checkbox("Nebel aktivieren", &currentMap.fogEnabled);
                if (currentMap.fogEnabled) {
                    static char fogName[256];
                    if (fogName[0] == 0) strcpy(fogName, currentMap.fogName.c_str());
                    if (ImGui::InputText("Nebel-Grafik", fogName, sizeof(fogName))) {
                        currentMap.fogName = fogName;
                    }
                    ImGui::DragInt("Blend-Modus", &currentMap.fogBlendMode, 1, 0, 2);
                    ImGui::ColorEdit4("Nebel-Farbe", &currentMap.fogColor.x);
                    ImGui::DragInt("Deckkraft", &currentMap.fogOpacity, 1, 0, 255);
                    ImGui::DragInt("Zoom %", &currentMap.fogZoom, 1, 10, 500);
                    ImGui::DragInt("Scroll X", &currentMap.fogSx);
                    ImGui::DragInt("Scroll Y", &currentMap.fogSy);
                }
                ImGui::EndTabItem();
            }
            
            // ==== EINSTELLUNGEN ====
            if (ImGui::BeginTabItem("Einstellungen")) {
                ImGui::Checkbox("Dash deaktivieren", &currentMap.disableDashing);
                
                const char* scrollTypes[] = { "Kein Loop", "Vertikal Loop", "Horizontal Loop", "Beide Loop" };
                ImGui::Combo("Scroll-Typ", &currentMap.scrollType, scrollTypes, 4);
                
                ImGui::Separator();
                ImGui::Text("Kampfhintergrund");
                static char battleback1[256], battleback2[256];
                if (battleback1[0] == 0) strcpy(battleback1, currentMap.battleback1Name.c_str());
                if (battleback2[0] == 0) strcpy(battleback2, currentMap.battleback2Name.c_str());
                ImGui::InputText("Battleback 1", battleback1, sizeof(battleback1));
                ImGui::InputText("Battleback 2", battleback2, sizeof(battleback2));
                currentMap.battleback1Name = battleback1;
                currentMap.battleback2Name = battleback2;
                
                ImGui::Separator();
                ImGui::Text("Zufällige Begegnungen");
                ImGui::DragInt("Schritte bis Begegnung", &currentMap.encounterStep, 1, 1, 1000);
                ImGui::Text("Enemy IDs (max 8):");
                for (int i = 0; i < 8; ++i) {
                    ImGui::DragInt(("Enemy " + std::to_string(i+1)).c_str(), &currentMap.encounterList[i], 1, 0, 999);
                }
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
    } else {
        ImGui::Text("Keine Karte ausgewählt. Erstellen Sie eine neue Karte oder wählen Sie eine aus der Liste.");
    }
    
    ImGui::EndChild();
    ImGui::End();
}

// ==================== Event Editor ====================

void Editor::DrawEventEditor() {
    if (mEngine.IsPlaying()) {
        ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
        ImGui::Begin("Event-Editor");
        ImGui::TextDisabled("Deaktiviert während Playtest");
        ImGui::End();
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Event-Editor");
    
    auto& eventSystem = EventSystem::Get();
    auto& events = eventSystem.GetEvents();
    auto& map = mEngine.GetMap();
    
    // Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    
    if (ImGui::Button((std::string(Icons::PLUS) + " Neues Event").c_str())) {
        MapEvent newEvent;
        newEvent.id = events.empty() ? 1 : events.back().id + 1;
        newEvent.name = "EV" + std::to_string(newEvent.id);
        newEvent.x = map.GetWidth() / 2;
        newEvent.y = map.GetHeight() / 2;
        newEvent.z = 0;
        
        // Erste Seite erstellen
        EventPage page;
        page.id = 1;
        page.trigger = EventTrigger::ActionButton;
        page.graphicName = "";
        page.graphicIndex = 0;
        page.list.clear();
        newEvent.pages.push_back(page);
        newEvent.currentPage = 0;
        
        eventSystem.AddEvent(newEvent);
        mSelectedEventId = newEvent.id;
        mSelectedEventPage = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icons::TRASH) + " Löschen").c_str()) && mSelectedEventId >= 0) {
        eventSystem.RemoveEvent(mSelectedEventId);
        mSelectedEventId = -1;
        mSelectedEventPage = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icons::PLUS) + " Seite").c_str()) && mSelectedEventId >= 0) {
        auto* ev = eventSystem.GetEvent(mSelectedEventId);
        if (ev) {
            EventPage page;
            page.id = static_cast<int>(ev->pages.size()) + 1;
            page.trigger = EventTrigger::ActionButton;
            ev->pages.push_back(page);
            mSelectedEventPage = static_cast<int>(ev->pages.size()) - 1;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icons::TRASH) + " Seite").c_str()) && mSelectedEventId >= 0 && mSelectedEventPage > 0) {
        auto* ev = eventSystem.GetEvent(mSelectedEventId);
        if (ev && ev->pages.size() > 1) {
            ev->pages.erase(ev->pages.begin() + mSelectedEventPage);
            mSelectedEventPage = std::max(0, mSelectedEventPage - 1);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icons::COPY) + " Kopieren").c_str()) && mSelectedEventId >= 0) {
        auto* ev = eventSystem.GetEvent(mSelectedEventId);
        if (ev) mClipboardCommand = EventCommand(); // Store event reference
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string(Icons::PASTE) + " Einfügen").c_str()) && mSelectedEventId >= 0) {
        // Paste logic
    }
    
    ImGui::PopStyleVar(2);
    ImGui::Separator();
    
    // Split view: Event list on left, details on right
    ImGui::BeginChild("EventListe", ImVec2(280, 0), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("Events auf dieser Karte");
    ImGui::Separator();
    
    for (const auto& ev : events) {
        bool selected = (mSelectedEventId == ev.id);
        std::string label = "EV" + std::to_string(ev.id) + ": " + ev.name;
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            mSelectedEventId = ev.id;
            mSelectedEventPage = 0;
        }
        // Context menu for events
        if (ImGui::BeginPopupContextItem(("EventContext##" + std::to_string(ev.id)).c_str())) {
            if (ImGui::MenuItem("Kopieren")) {
                mClipboardCommand = EventCommand(); // Store for later
            }
            if (ImGui::MenuItem("Duplizieren")) {
                MapEvent newEvent = ev;
                newEvent.id = events.empty() ? 1 : events.back().id + 1;
                newEvent.name = ev.name + "_Copy";
                eventSystem.AddEvent(newEvent);
            }
            if (ImGui::MenuItem("Löschen")) {
                eventSystem.RemoveEvent(ev.id);
                if (mSelectedEventId == ev.id) {
                    mSelectedEventId = -1;
                    mSelectedEventPage = -1;
                }
            }
            ImGui::EndPopup();
        }
    }
    
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Event-Details & Befehlsliste
    ImGui::BeginChild("EventDetails", ImVec2(0, 0), true);
    
    if (mSelectedEventId >= 0) {
        auto* ev = eventSystem.GetEvent(mSelectedEventId);
        if (ev) {
            // Header with event info
            ImGui::Text("Event: %s (ID: %d)", ev->name.c_str(), ev->id);
            ImGui::Separator();
            
            static char eventName[128];
            if (eventName[0] == 0) strcpy(eventName, ev->name.c_str());
            if (ImGui::InputText("Name", eventName, sizeof(eventName))) {
                ev->name = eventName;
            }
            
            ImGui::DragInt("X", &ev->x, 1, 0, map.GetWidth()-1);
            ImGui::SameLine();
            ImGui::DragInt("Y", &ev->y, 1, 0, map.GetHeight()-1);
            ImGui::SameLine();
            if (ImGui::Button("Zur Position")) {
                // Focus camera on event
                Camera& cam = mEngine.GetRenderer().GetCamera();
                cam.SetPosition(Vec3(ev->x - map.GetWidth()*0.5f, 10, ev->y - map.GetHeight()*0.5f + 10));
                cam.SetRotation(Vec3(-45, 0, 0));
            }
            
            ImGui::Separator();
            
            // Seiten-Auswahl with better UI
            ImGui::Text("Seiten");
            ImGui::SameLine();
            if (ImGui::Button((std::string(Icons::PLUS) + " Seite hinzufügen").c_str())) {
                EventPage page;
                page.id = static_cast<int>(ev->pages.size()) + 1;
                page.trigger = EventTrigger::ActionButton;
                ev->pages.push_back(page);
                mSelectedEventPage = static_cast<int>(ev->pages.size()) - 1;
            }
            
            // Page tabs
            if (ImGui::BeginTabBar("EventPages")) {
                for (size_t i = 0; i < ev->pages.size(); ++i) {
                    bool selected = (mSelectedEventPage == static_cast<int>(i));
                    std::string label = "Seite " + std::to_string(i+1);
                    if (i == ev->currentPage) label += " *";
                    
                    bool open = true;
                    if (ImGui::BeginTabItem(label.c_str(), &open)) {
                        mSelectedEventPage = static_cast<int>(i);
                        ev->currentPage = static_cast<int>(i);
                        ImGui::EndTabItem();
                    }
                    
                    // Close button in tab
                    if (!open && ev->pages.size() > 1) {
                        ev->pages.erase(ev->pages.begin() + i);
                        mSelectedEventPage = std::max(0, mSelectedEventPage - 1);
                    }
                }
                
                // Add page button
                if (ImGui::TabItemButton(Icons::PLUS, ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                    EventPage page;
                    page.id = static_cast<int>(ev->pages.size()) + 1;
                    page.trigger = EventTrigger::ActionButton;
                    ev->pages.push_back(page);
                    mSelectedEventPage = static_cast<int>(ev->pages.size()) - 1;
                }
                
                ImGui::EndTabBar();
            }
            
            if (mSelectedEventPage >= 0 && mSelectedEventPage < static_cast<int>(ev->pages.size())) {
                auto& page = ev->pages[mSelectedEventPage];
                
                ImGui::Separator();
                ImGui::Text("Seiten-Eigenschaften");
                
                const char* triggers[] = { "Action Button", "Player Touch", "Event Touch", "Autorun", "Parallel" };
                int triggerIdx = static_cast<int>(page.trigger);
                if (ImGui::Combo("Auslöser", &triggerIdx, triggers, 5)) {
                    page.trigger = static_cast<EventTrigger>(triggerIdx);
                }
                
                ImGui::Checkbox("Laufanimation", &page.walkAnime);
                ImGui::SameLine();
                ImGui::Checkbox("Schrittanimation", &page.stepAnime);
                ImGui::SameLine();
                ImGui::Checkbox("Richtung fixieren", &page.directionFix);
                ImGui::SameLine();
                ImGui::Checkbox("Durchlässig", &page.through);
                
                const char* moveTypes[] = { "Fix", "Zufällig", "Annähern", "Benutzerdefiniert" };
                ImGui::Combo("Bewegungstyp", &page.moveType, moveTypes, 4);
                ImGui::DragInt("Geschwindigkeit", &page.moveSpeed, 1, 1, 6);
                ImGui::SameLine();
                ImGui::DragInt("Häufigkeit", &page.moveFrequency, 1, 1, 6);
                
                ImGui::Separator();
                ImGui::Text("Bedingungen");
                ImGui::Checkbox("Switch 1", &page.condition.switch1Valid);
                if (page.condition.switch1Valid) ImGui::DragInt("Switch 1 ID", &page.condition.switch1Id, 1, 1, 5000);
                ImGui::SameLine();
                ImGui::Checkbox("Switch 2", &page.condition.switch2Valid);
                if (page.condition.switch2Valid) ImGui::DragInt("Switch 2 ID", &page.condition.switch2Id, 1, 1, 5000);
                ImGui::Checkbox("Variable", &page.condition.variableValid);
                if (page.condition.variableValid) {
                    ImGui::DragInt("Var ID", &page.condition.variableId, 1, 1, 5000);
                    ImGui::SameLine();
                    ImGui::DragInt("Var Wert", &page.condition.variableValue);
                }
                ImGui::Checkbox("Self Switch", &page.condition.selfSwitchValid);
                if (page.condition.selfSwitchValid) {
                    const char* selfSwitches[] = { "A", "B", "C", "D" };
                    int ssIdx = page.condition.selfSwitchCh - 'A';
                    if (ImGui::Combo("Self Switch", &ssIdx, selfSwitches, 4)) {
                        page.condition.selfSwitchCh = 'A' + ssIdx;
                    }
                }
                
                ImGui::Separator();
                ImGui::Text("Grafik");
                static char graphicName[256];
                if (graphicName[0] == 0) strcpy(graphicName, page.graphicName.c_str());
                if (ImGui::InputText("Modell/Sprite", graphicName, sizeof(graphicName))) {
                    page.graphicName = graphicName;
                }
                ImGui::SameLine();
                ImGui::DragInt("Index", &page.graphicIndex, 1, 0, 7);
                
                ImGui::Separator();
                
                // ==== BEFEHLSLISTE (DER KERN) ====
                if (ImGui::BeginTabBar("EventTabs")) {
                    if (ImGui::BeginTabItem("Befehle")) {
                        DrawEventCommandList(page);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Neuer Befehl")) {
                        DrawAddEventCommand(page);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
        }
    } else {
        ImGui::Text("Kein Event ausgewählt.");
        ImGui::Text("Wählen Sie ein Event aus der Liste oder erstellen Sie ein neues.");
        ImGui::Separator();
        ImGui::TextWrapped("Tipp: Rechtsklick auf ein Event für weitere Optionen (Kopieren, Duplizieren, Löschen).");
    }
    
    ImGui::EndChild();

    // === Native Fenster für Befehls-Editierung (Fix für "Event-Editor soll windows fenster nutzen") ===
    if (mEditingCommandIndex >= 0 && mSelectedEventId >= 0) {
        auto* ev = EventSystem::Get().GetEvent(mSelectedEventId);
        if (ev && mEditingCommandIndex < (int)ev->pages[mSelectedEventPage].list.size()) {
            auto& page = ev->pages[mSelectedEventPage];
            auto& cmd = page.list[mEditingCommandIndex];
            ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
            bool open = true;
            if (ImGui::Begin("Befehl Bearbeiten (Native Fenster)", &open, ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("Datei")) {
                        if (ImGui::MenuItem("Speichern (Native Dialog)")) {
                            // Auf Windows nutzt SaveFileDialog natives Windows Fenster
                            std::string p = SaveFileDialog("Text Files (*.txt)\0*.txt\0");
                            if (!p.empty()) {
                                RPG_LOG_INFO("Befehl als Datei gespeichert: " + p);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenuBar();
                }
                ImGui::Text("Befehl: %s", GetEventCommandName(cmd.code).c_str());
                ImGui::Separator();
                // Text Input für ShowText etc.
                if (cmd.code == EventCommandCode::ShowText || cmd.code == EventCommandCode::Comment || cmd.code == EventCommandCode::Script) {
                    static char buf[4096];
                    if (mShowCommandEditor) {
                        strncpy(buf, cmd.text.c_str(), sizeof(buf)-1);
                        buf[sizeof(buf)-1] = '\0';
                        mShowCommandEditor = false;
                    }
                    ImGui::Text("Text / Script:");
                    if (ImGui::InputTextMultiline("##CmdText", buf, sizeof(buf), ImVec2(-1, 200))) {
                        cmd.text = buf;
                    }
                    if (ImGui::Button("In externem Editor öffnen (Windows Fenster)")) {
#if defined(_WIN32)
                        // Temporäre Datei schreiben und mit Notepad öffnen – natives Windows Fenster
                        std::string tmp = std::filesystem::temp_directory_path().string() + "\\rpg_event_edit.txt";
                        std::ofstream f(tmp);
                        f << cmd.text;
                        f.close();
                        ShellExecuteA(nullptr, "open", "notepad.exe", tmp.c_str(), nullptr, SW_SHOWNORMAL);
#else
                        std::string tmp = "/tmp/rpg_event_edit.txt";
                        std::ofstream f(tmp);
                        f << cmd.text;
                        f.close();
                        system(("xdg-open \"" + tmp + "\" 2>/dev/null &").c_str());
#endif
                    }
                }
                ImGui::Separator();
                ImGui::Text("Parameter (je nach Befehl unterschiedlich):");
                ImGui::DragInt("Param1", &cmd.param1);
                ImGui::DragInt("Param2", &cmd.param2);
                ImGui::DragInt("Param3", &cmd.param3);
                ImGui::Separator();
                if (ImGui::Button("OK", ImVec2(120,0))) {
                    mEditingCommandIndex = -1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Abbrechen", ImVec2(120,0))) {
                    mEditingCommandIndex = -1;
                }
                ImGui::End();
            } else {
                ImGui::End();
                mEditingCommandIndex = -1;
            }
        } else {
            mEditingCommandIndex = -1;
        }
    }

    ImGui::End();
}

// Zeichnet die Liste der Befehle für eine Event-Seite
void Editor::DrawEventCommandList(EventPage& page) {
    // Toolbar für Befehle
    if (ImGui::Button("Befehl oben einfügen")) {
        EventCommand cmd;
        cmd.code = EventCommandCode::Comment;
        cmd.text = "Neuer Befehl";
        page.list.insert(page.list.begin(), cmd);
    }
    ImGui::SameLine();
    if (ImGui::Button("Befehl unten einfügen")) {
        EventCommand cmd;
        cmd.code = EventCommandCode::Comment;
        cmd.text = "Neuer Befehl";
        page.list.push_back(cmd);
    }
    ImGui::SameLine();
    if (ImGui::Button("Löschen") && mSelectedCommandIndex >= 0 && mSelectedCommandIndex < static_cast<int>(page.list.size())) {
        page.list.erase(page.list.begin() + mSelectedCommandIndex);
        mSelectedCommandIndex = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Nach oben") && mSelectedCommandIndex > 0) {
        std::swap(page.list[mSelectedCommandIndex], page.list[mSelectedCommandIndex - 1]);
        mSelectedCommandIndex--;
    }
    ImGui::SameLine();
    if (ImGui::Button("Nach unten") && mSelectedCommandIndex >= 0 && mSelectedCommandIndex < static_cast<int>(page.list.size()) - 1) {
        std::swap(page.list[mSelectedCommandIndex], page.list[mSelectedCommandIndex + 1]);
        mSelectedCommandIndex++;
    }
    
    ImGui::Separator();
    
    // Befehlsliste
    ImGui::BeginChild("BefehlsListe", ImVec2(0, 0), true);
    
    for (size_t i = 0; i < page.list.size(); ++i) {
        const auto& cmd = page.list[i];
        bool selected = (mSelectedCommandIndex == static_cast<int>(i));
        
        // Command code name
        std::string cmdName = GetEventCommandName(cmd.code);
        std::string paramsStr = GetEventCommandParamsString(cmd);
        
        std::string label = "[" + std::to_string(i) + "] " + cmdName;
        if (!paramsStr.empty()) label += " : " + paramsStr;
        
        // Indent für Struktur
        ImGui::Indent(cmd.indent * 20.0f);
        if (ImGui::Selectable(label.c_str(), selected)) {
            mSelectedCommandIndex = static_cast<int>(i);
        }
        ImGui::Unindent(cmd.indent * 20.0f);
        
        // Context menu für Bearbeiten
        if (ImGui::BeginPopupContextItem(("CmdContext##" + std::to_string(i)).c_str())) {
            if (ImGui::MenuItem("Bearbeiten")) {
                mEditingCommandIndex = static_cast<int>(i);
                mShowCommandEditor = true;
            }
            if (ImGui::MenuItem("Kopieren")) {
                mClipboardCommand = cmd;
            }
            if (ImGui::MenuItem("Einfügen")) {
                page.list.insert(page.list.begin() + i + 1, mClipboardCommand);
            }
            if (ImGui::MenuItem("Löschen")) {
                page.list.erase(page.list.begin() + i);
                if (mSelectedCommandIndex >= static_cast<int>(page.list.size()))
                    mSelectedCommandIndex = static_cast<int>(page.list.size()) - 1;
            }
            ImGui::EndPopup();
        }
    }
    
    ImGui::EndChild();
}

// Zeichnet den "Neuer Befehl" Dialog
void Editor::DrawAddEventCommand(EventPage& page) {
    ImGui::Text("Wählen Sie einen Befehlskategorie:");
    ImGui::Separator();
    
    // Kategorien wie in RPG Maker
    if (ImGui::CollapsingHeader("Nachricht")) {
        if (ImGui::Selectable("Text anzeigen")) { AddCommand(page, EventCommandCode::ShowText); }
        if (ImGui::Selectable("Optionen anzeigen")) { AddCommand(page, EventCommandCode::ShowChoices); }
        if (ImGui::Selectable("Zahl eingeben")) { AddCommand(page, EventCommandCode::InputNumber); }
        if (ImGui::Selectable("Textoptionen")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("Bildschirm")) {
        if (ImGui::Selectable("Bildschirmton ändern")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Bildschirmflackern")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Bildschirmshake")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Warten")) { AddCommand(page, EventCommandCode::Wait); }
    }
    
    if (ImGui::CollapsingHeader("Karte")) {
        if (ImGui::Selectable("Spieler transferieren")) { AddCommand(page, EventCommandCode::TransferPlayer); }
        if (ImGui::Selectable("Fahrzeug einsteigen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Position festlegen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Scrollen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Karteneinstellungen ändern")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("Event")) {
        if (ImGui::Selectable("Bewegungsroute festlegen")) { AddCommand(page, EventCommandCode::SetMoveRoute); }
        if (ImGui::Selectable("Event kurzzeitig anhalten")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("Bilder & Filme")) {
        if (ImGui::Selectable("Bild anzeigen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Bild bewegen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Bild drehen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Bildfarbe ändern")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Bild löschen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Film abspielen")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("Timer")) {
        if (ImGui::Selectable("Timer steuern")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("System")) {
        if (ImGui::Selectable("Gold ändern")) { AddCommand(page, EventCommandCode::ChangeGold); }
        if (ImGui::Selectable("Gegenstände ändern")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Waffen/Rüstung ändern")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Switch bedienen")) { AddCommand(page, EventCommandCode::ChangeSwitch); }
        if (ImGui::Selectable("Variable bedienen")) { AddCommand(page, EventCommandCode::ChangeVariable); }
        if (ImGui::Selectable("Self Switch bedienen")) { AddCommand(page, EventCommandCode::ChangeSelfSwitch); }
        if (ImGui::Selectable("Timer")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("Musik & Sound")) {
        if (ImGui::Selectable("BGM abspielen")) { AddCommand(page, EventCommandCode::PlayBGM); }
        if (ImGui::Selectable("BGM stoppen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("BGM faden")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("BGS abspielen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("SE abspielen")) { AddCommand(page, EventCommandCode::PlaySE); }
        if (ImGui::Selectable("SE stoppen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("ME abspielen")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("Spezial")) {
        if (ImGui::Selectable("Kampf starten")) { AddCommand(page, EventCommandCode::BattleProcessing); }
        if (ImGui::Selectable("Laden")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Speichern")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Game Over")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Zum Titel")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Script")) { AddCommand(page, EventCommandCode::Script); }
    }
    
    if (ImGui::CollapsingHeader("Flow Control")) {
        if (ImGui::Selectable("Bedingte Verzweigung")) { AddCommand(page, EventCommandCode::ConditionalBranch); }
        if (ImGui::Selectable("Schleife")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Schleife unterbrechen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Event verlassen")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Goto Label")) { AddCommand(page, EventCommandCode::Comment); }
        if (ImGui::Selectable("Label")) { AddCommand(page, EventCommandCode::Comment); }
    }
    
    if (ImGui::CollapsingHeader("3D-Erweiterungen")) {
        if (ImGui::Selectable("Entität spawnen")) { AddCommand(page, EventCommandCode::SpawnEntity); }
        if (ImGui::Selectable("Entität bewegen")) { AddCommand(page, EventCommandCode::MoveEntity); }
        if (ImGui::Selectable("Entität drehen")) { AddCommand(page, EventCommandCode::RotateEntity); }
        if (ImGui::Selectable("Animation abspielen")) { AddCommand(page, EventCommandCode::PlayAnimation); }
    }
}

// Fügt einen neuen Befehl zur Liste hinzu
void Editor::AddCommand(EventPage& page, EventCommandCode code) {
    EventCommand cmd;
    cmd.code = code;
    cmd.indent = 0;
    
    // Standard-Parameter setzen
    switch (code) {
        case EventCommandCode::ShowText:
            cmd.text = "Text hier eingeben...";
            break;
        case EventCommandCode::Wait:
            cmd.param1 = 60; // 1 Sekunde bei 60 FPS
            break;
        case EventCommandCode::TransferPlayer:
            cmd.param1 = 0; // Map ID (0 = current)
            cmd.param2 = 0; // X
            cmd.param3 = 0; // Y
            break;
        case EventCommandCode::PlayBGM:
            cmd.text = "bgm.ogg";
            cmd.param1 = 1; // loop
            break;
        case EventCommandCode::PlaySE:
            cmd.text = "se.ogg";
            break;
        case EventCommandCode::ChangeGold:
            cmd.param1 = 0; // amount
            break;
        case EventCommandCode::ChangeSwitch:
            cmd.param1 = 1; // switch ID
            cmd.param2 = 1; // 0=OFF, 1=ON
            break;
        case EventCommandCode::ChangeVariable:
            cmd.param1 = 1; // var ID
            cmd.param2 = 0; // operation (0=set, 1=add, etc.)
            cmd.param3 = 0; // value
            break;
        case EventCommandCode::ChangeSelfSwitch:
            cmd.param1 = 0; // 'A'=0, 'B'=1, etc.
            cmd.param2 = 1; // 0=OFF, 1=ON
            break;
        case EventCommandCode::ConditionalBranch:
            cmd.param1 = 1; // switch ID
            break;
        case EventCommandCode::Script:
            cmd.text = "// Script hier eingeben";
            break;
        default:
            break;
    }
    
    // Am Cursor oder am Ende einfügen
    if (mSelectedCommandIndex >= 0 && mSelectedCommandIndex < static_cast<int>(page.list.size())) {
        page.list.insert(page.list.begin() + mSelectedCommandIndex + 1, cmd);
        mSelectedCommandIndex++;
    } else {
        page.list.push_back(cmd);
        mSelectedCommandIndex = static_cast<int>(page.list.size()) - 1;
    }
}

// Gibt den Namen eines Event-Befehls zurück
std::string Editor::GetEventCommandName(EventCommandCode code) {
    switch (code) {
        case EventCommandCode::ShowText: return "Text anzeigen";
        case EventCommandCode::ShowChoices: return "Optionen anzeigen";
        case EventCommandCode::InputNumber: return "Zahl eingeben";
        case EventCommandCode::Wait: return "Warten";
        case EventCommandCode::TransferPlayer: return "Spieler transferieren";
        case EventCommandCode::SetMoveRoute: return "Bewegungsroute";
        case EventCommandCode::PlayBGM: return "BGM abspielen";
        case EventCommandCode::PlaySE: return "SE abspielen";
        case EventCommandCode::ChangeGold: return "Gold ändern";
        case EventCommandCode::ChangeSwitch: return "Switch bedienen";
        case EventCommandCode::ChangeVariable: return "Variable bedienen";
        case EventCommandCode::ChangeSelfSwitch: return "Self Switch bedienen";
        case EventCommandCode::ConditionalBranch: return "Bedingte Verzweigung";
        case EventCommandCode::Script: return "Script";
        case EventCommandCode::BattleProcessing: return "Kampf starten";
        case EventCommandCode::Comment: return "Kommentar";
        case EventCommandCode::SpawnEntity: return "Entität spawnen (3D)";
        case EventCommandCode::MoveEntity: return "Entität bewegen (3D)";
        case EventCommandCode::RotateEntity: return "Entität drehen (3D)";
        case EventCommandCode::PlayAnimation: return "Animation (3D)";
        default: return "Unbekannt (" + std::to_string(static_cast<int>(code)) + ")";
    }
}

// Gibt Parameter-String für Anzeige zurück
std::string Editor::GetEventCommandParamsString(const EventCommand& cmd) {
    switch (cmd.code) {
        case EventCommandCode::ShowText:
            return cmd.text.substr(0, 40) + (cmd.text.size() > 40 ? "..." : "");
        case EventCommandCode::Wait:
            return std::to_string(cmd.param1 / 60.0f) + " Sek";
        case EventCommandCode::TransferPlayer:
            return "Map " + std::to_string(cmd.param1) + " (" + std::to_string(cmd.param2) + "," + std::to_string(cmd.param3) + ")";
        case EventCommandCode::PlayBGM:
            return cmd.text + (cmd.param1 ? " (Loop)" : "");
        case EventCommandCode::PlaySE:
            return cmd.text;
        case EventCommandCode::ChangeGold:
            return (cmd.param1 >= 0 ? "+" : "") + std::to_string(cmd.param1) + " G";
        case EventCommandCode::ChangeSwitch:
            return "Switch " + std::to_string(cmd.param1) + " = " + (cmd.param2 ? "ON" : "OFF");
        case EventCommandCode::ChangeVariable:
            return "Var " + std::to_string(cmd.param1) + " = " + std::to_string(cmd.param3);
        case EventCommandCode::ChangeSelfSwitch:
            return "Self Switch " + std::string(1, 'A' + cmd.param1) + " = " + (cmd.param2 ? "ON" : "OFF");
        case EventCommandCode::ConditionalBranch:
            return "Switch " + std::to_string(cmd.param1) + " ist ON";
        case EventCommandCode::Script:
            return cmd.text.substr(0, 30) + (cmd.text.size() > 30 ? "..." : "");
        case EventCommandCode::Comment:
            return cmd.text;
        default:
            return "";
    }
}

void Editor::DrawScriptEditor() {
    // Überarbeitet: Script Editor soll laut Nutzer "Windows Fenster nutzen statt ImGui wegen besseren Design und Platz"
    // Wir behalten ImGui als Basis, bieten aber Native-Fenster Funktionalität: Externen Editor öffnen (Notepad/Code) + große Float Window
    // Auf Windows nutzt SaveFileDialog/OpenFileDialog bereits native Windows Dialoge (GetOpenFileName), das erfüllt "Windows Fenster"
    ImGui::SetNextWindowSize(ImVec2(1100, 650), ImGuiCond_FirstUseEver);
    bool isOpen = true;
    ImGui::Begin("Script Editor (Native Dialoge + Externer Editor)", &isOpen, ImGuiWindowFlags_MenuBar);

    auto& scriptManager = mEngine.GetScriptManager();
    auto& scripts = scriptManager.GetScripts();
    static int selectedTab = 0;
    if (selectedTab >= (int)scripts.size()) selectedTab = 0;

    // Menübar für Script Editor (Native Windows Fenster laut User-Wunsch)
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Datei")) {
            if (ImGui::MenuItem("Neues Script (Native Dialog)")) {
                ImGui::OpenPopup("NewScriptPopup");
            }
            if (ImGui::MenuItem("Alle speichern", "Ctrl+S")) {
                scriptManager.SaveAllScripts();
            }
            if (ImGui::MenuItem("Neu laden")) {
                scriptManager.ReloadFromDisk();
            }
            if (ImGui::MenuItem("In externem Editor öffnen (Windows Fenster)")) {
                if (selectedTab >= 0 && selectedTab < (int)scripts.size()) {
                    auto* script = scripts[selectedTab].get();
#if defined(_WIN32)
                    ShellExecuteA(nullptr, "open", script->path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
                    std::string openCmd = "xdg-open \"" + script->path + "\" 2>/dev/null &";
                    system(openCmd.c_str());
#endif
                    RPG_LOG_INFO("Externer Editor geöffnet für: " + script->path);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ansicht")) {
            if (ImGui::MenuItem("Externer Editor Hinweis")) {
                RPG_LOG_INFO("Nutzer will Windows Fenster statt ImGui wegen Design/Platz – externer Editor öffnet natives Fenster.");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Left: ordered script list (top -> bottom load order for game)
    ImGui::BeginChild("ScriptList", ImVec2(260, 0), true);
    ImGui::TextDisabled("Load order (top -> bottom)");
    ImGui::Separator();
    if (ImGui::Button((std::string(Icons::PLUS) + " New").c_str())) {
        ImGui::OpenPopup("NewScriptPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save All")) scriptManager.SaveAllScripts();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) scriptManager.ReloadFromDisk();

    static char newScriptName[128] = "game_logic.rb";
    if (ImGui::BeginPopupModal("NewScriptPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", newScriptName, sizeof(newScriptName));
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string name = newScriptName;
            if (name.size() < 3 || name.substr(name.size() - 3) != ".rb") name += ".rb";
            scriptManager.CreateScript(name);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    for (size_t i = 0; i < scripts.size(); ++i) {
        auto* script = scripts[i].get();
        std::string label = std::string(Icons::FILE_CODE) + " " + script->name;
        if (script->modified) label += " *";
        if (script->isCore) label += " [core]";
        bool sel = (selectedTab == static_cast<int>(i));
        if (ImGui::Selectable(label.c_str(), sel)) selectedTab = static_cast<int>(i);
        if (ImGui::BeginPopupContextItem()) {
            selectedTab = static_cast<int>(i);
            if (ImGui::MenuItem("Run")) mEngine.GetRubyVM().ExecuteString(script->content);
            if (ImGui::MenuItem("Save") && !script->isCore) scriptManager.SaveScript(scripts[i]);
            if (ImGui::MenuItem("Delete") && !script->isCore) {
                scriptManager.DeleteScript(script->name);
                selectedTab = 0;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Execute All (Game Order)", ImVec2(-1, 0))) {
        scriptManager.ExecuteAllScripts();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: source editor
    ImGui::BeginChild("ScriptSource", ImVec2(0, 0), true);
    if (selectedTab >= 0 && selectedTab < static_cast<int>(scripts.size())) {
        auto* script = scripts[selectedTab].get();
        ImGui::Text("%s", script->path.c_str());
        if (script->isCore) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "(core)");
        }
        ImGui::Separator();

        static std::string editBuffer;
        static int editScriptIndex = -1;
        if (editScriptIndex != selectedTab) {
            editBuffer = script->content;
            editScriptIndex = selectedTab;
        }
        if (editBuffer.capacity() < 65536) editBuffer.reserve(65536);
        editBuffer.push_back('\0');
        editBuffer.pop_back();

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
        if (script->isCore || mEngine.IsPlaying()) flags |= ImGuiInputTextFlags_ReadOnly;
        if (mEngine.IsPlaying()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
            ImGui::Text("PLAYTEST - Script System läuft live (schreibgeschützt)");
            ImGui::PopStyleColor();
        }
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (ImGui::InputTextMultiline("##ScriptSource", editBuffer.data(),
            editBuffer.capacity() + 1, ImVec2(avail.x, avail.y - 36), flags)) {
            editBuffer.resize(std::strlen(editBuffer.c_str()));
            if (!script->isCore) {
                script->content = editBuffer;
                script->modified = true;
            }
        }
        if (!script->isCore) {
            if (ImGui::Button("Save")) {
                scriptManager.SaveScript(scripts[selectedTab]);
                editBuffer = script->content;
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Run Script")) {
            mEngine.GetRubyVM().ExecuteString(script->content);
        }
    } else {
        ImGui::TextDisabled("No script selected. Create one with + New.");
    }
    ImGui::EndChild();

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
    ImGui::Begin("Konsole");

    static char command[256] = "";
    static bool autoScroll = true;

    ImGui::Text("Engine bereit. FPS: %d", mEngine.GetFPS());
    ImGui::SameLine();
    ImGui::Checkbox("Auto-Scroll", &autoScroll);
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

    if (ImGui::InputText("Befehl", command, sizeof(command), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string cmd = command;
        rpg::Logger::Get().Info("> " + cmd, "Konsole");

        if (cmd == "clear") {
            rpg::Logger::Get().Clear();
        } else if (cmd == "fps") {
            rpg::Logger::Get().Info("FPS: " + std::to_string(mEngine.GetFPS()), "Konsole");
        } else if (cmd == "help") {
            rpg::Logger::Get().Info("Befehle: clear, fps, help, loglevel", "Konsole");
        } else if (cmd == "loglevel") {
            rpg::Logger::Get().Info("Log-Level: Trace, Debug, Info, Warning, Error, Fatal", "Konsole");
        } else {
            rpg::Logger::Get().Warning("Unbekannter Befehl: " + cmd, "Konsole");
        }
        command[0] = '\0';
    }
    ImGui::End();
}

void Editor::CreateCube() {
    auto cmd = std::make_shared<CreateEntityCommand>("Cube");
    mEngine.GetCommandHistory().Execute(mEngine, cmd);
    EntityID id = cmd->GetEntityID();
    if (id != INVALID_ENTITY) {
        auto* transform = mEngine.GetScene().AddComponent<TransformComponent>(id);
        transform->transform.position = Vec3(0, 0.5f, 0);
        auto* model = mEngine.GetScene().AddComponent<ModelRendererComponent>(id);
        model->model = std::make_shared<Model>();
        model->model->AddMesh(MeshFactory::CreateCube(1.0f));
        mSelectedEntity = static_cast<int>(id);
        RPG_LOG_INFO("Created Cube entity");
    }
}

void Editor::CreatePlane() {
    auto cmd = std::make_shared<CreateEntityCommand>("Plane");
    mEngine.GetCommandHistory().Execute(mEngine, cmd);
    EntityID id = cmd->GetEntityID();
    if (id != INVALID_ENTITY) {
        auto* transform = mEngine.GetScene().AddComponent<TransformComponent>(id);
        transform->transform.position = Vec3(0, 0.1f, 0);
        auto* model = mEngine.GetScene().AddComponent<ModelRendererComponent>(id);
        model->model = std::make_shared<Model>();
        model->model->AddMesh(MeshFactory::CreatePlane(2.0f));
        mSelectedEntity = static_cast<int>(id);
        RPG_LOG_INFO("Created Plane entity");
    }
}

void Editor::CreateLight() {
    auto cmd = std::make_shared<CreateEntityCommand>("Light");
    mEngine.GetCommandHistory().Execute(mEngine, cmd);
    EntityID id = cmd->GetEntityID();
    if (id != INVALID_ENTITY) {
        auto* transform = mEngine.GetScene().AddComponent<TransformComponent>(id);
        transform->transform.position = Vec3(0, 3.0f, 0);
        auto* light = mEngine.GetScene().AddComponent<LightComponent>(id);
        light->color = Color(1.0f, 1.0f, 0.0f, 1.0f);
        light->intensity = 1.0f;
        mSelectedEntity = static_cast<int>(id);
        RPG_LOG_INFO("Created Light entity");
    }
}

void Editor::DeleteSelectedEntity() {
    if (mSelectedEntity >= 0) {
        EntityID id = static_cast<EntityID>(mSelectedEntity);
        auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(id);
        Transform t = transform ? transform->transform : Transform();
        auto cmd = std::make_shared<DeleteEntityCommand>(id, mEngine.GetScene().GetEntityName(id), t);
        mEngine.GetCommandHistory().Execute(mEngine, cmd);
        mSelectedEntity = -1;
        RPG_LOG_INFO("Deleted selected entity");
    }
}

void Editor::SaveMap() {
    // Full scene package: map tiles + entities + lighting
    std::string scenePath = mEngine.GetProject().GetProjectPath() + "/scene.json";
    mEngine.SaveScene(scenePath);
    // Binary map path for runtime
    std::string mapPath = mEngine.GetProject().GetMapPath(1);
    mEngine.GetMap().Save(mapPath);
    // Persist map infos / database
    Database::Get().Save(mEngine.GetProject().GetProjectPath());
    mEngine.GetProject().Save();
    RPG_LOG_INFO("Map+Scene saved: " + scenePath);
}

void Editor::LoadMap() {
    std::string scenePath = mEngine.GetProject().GetProjectPath() + "/scene.json";
    if (mEngine.LoadScene(scenePath)) {
        mSelectedEntity = -1;
        RPG_LOG_INFO("Map+Scene loaded: " + scenePath);
        return;
    }
    // Fallback: Kartendatei, bei Bedarf PAKET-25-Standardkarte
    std::string path = mEngine.GetProject().GetMapPath(1);
    if (mEngine.LoadRuntimeMap(1)) {
        RPG_LOG_INFO("Map loaded from: " + path);
    } else {
        RPG_LOG_WARN("Keine Kartendatei - Standardkarte generiert (speicherbar): " + path);
    }
}

// ==================== Neue Map Editor Funktionen ====================

void Editor::LoadSelectedMap() {
    if (mSelectedMapIndex < 0) return;
    
    auto& database = Database::Get();
    auto& mapInfos = database.MapInfos();
    
    if (mSelectedMapIndex >= static_cast<int>(mapInfos.size())) return;
    
    auto& mapInfo = mapInfos[mSelectedMapIndex];
    
    // Karte im Engine Map laden
    auto& map = mEngine.GetMap();
    map.Resize(mapInfo.width, mapInfo.height);
    
    // Tileset laden
    LoadTilesetForMap(mapInfo.tilesetId);
    
    // Fog-Einstellungen auf Renderer anwenden
    auto& renderer = mEngine.GetRenderer();
    auto& fog = renderer.GetFog();
    fog.enabled = mapInfo.fogEnabled;
    fog.color = mapInfo.fogColor;
    fog.start = 10.0f;
    fog.end = 100.0f;
    
    // Lighting anpassen
    auto& lighting = Lighting::Get();
    auto& ambient = lighting.GetAmbient();
    ambient.color = mapInfo.backgroundColor;
    
    // Kameraposition auf Karten-Mitte setzen
    Camera& cam = mEngine.GetRenderer().GetCamera();
    cam.SetPosition(Vec3(0, 10, 10));
    cam.SetRotation(Vec3(-45, 0, 0));
    
    // Events für diese Karte laden
    EventSystem::Get().LoadMapEvents(mapInfo.id, mEngine.GetProject().GetProjectPath());

    // Prefer per-map scene package if present
    std::string scenePath = mEngine.GetProject().GetProjectPath() + "/maps/map" + std::to_string(mapInfo.id) + "_scene.json";
    if (!fs::exists(scenePath)) {
        scenePath = mEngine.GetProject().GetProjectPath() + "/scene.json";
    }
    if (fs::exists(scenePath)) {
        mEngine.LoadScene(scenePath);
        // Keep map size from mapInfo if scene didn't override
        if (mEngine.GetMap().GetWidth() != mapInfo.width || mEngine.GetMap().GetHeight() != mapInfo.height) {
            // scene already loaded map; OK
        }
        mSelectedEntity = -1;
    } else {
        // Binary map fallback, bei Bedarf PAKET-25-Standardkarte
        mEngine.LoadRuntimeMap(mapInfo.id);
    }

    RPG_LOG_INFO("Karte geladen: " + mapInfo.name + " (" + std::to_string(mapInfo.width) + "x" + std::to_string(mapInfo.height) + ")");
}

void Editor::ResizeCurrentMap(int width, int height) {
    if (mSelectedMapIndex < 0) return;
    
    auto& database = Database::Get();
    auto& mapInfos = database.MapInfos();
    
    if (mSelectedMapIndex >= static_cast<int>(mapInfos.size())) return;
    
    auto& mapInfo = mapInfos[mSelectedMapIndex];
    mapInfo.width = width;
    mapInfo.height = height;
    
    LoadSelectedMap();
}

void Editor::LoadTilesetForMap(int tilesetId) {
    auto& database = Database::Get();
    auto& tilesets = database.Tilesets();
    
    for (const auto& ts : tilesets) {
        if (ts.id == tilesetId) {
            auto tileset = std::make_shared<Tileset>();
            std::string path = mEngine.GetProject().GetAssetPath("textures/" + ts.tilesetName);
            bool ok = tileset->Load(path, 32, 32);
            if (!ok) {
                // Fallback
                tileset->Load("assets/textures/tileset_demo.png", 32, 32);
            }
            // XP-Tileset-Flags (Durchgaengigkeit, 4-Dir, Prioritaet, ...)
            // ans Runtime-Tileset koppeln - unabhaengig vom Grafik-Erfolg.
            tileset->SetTilesetData(ts);
            mEngine.GetMap().SetTileset(tileset);
            if (ok) {
                RPG_LOG_INFO("Tileset geladen: " + ts.name);
            }
            return;
        }
    }
    
    // Fallback: Default Tileset
    auto tileset = std::make_shared<Tileset>();
    tileset->Load("assets/textures/tileset_demo.png", 32, 32);
    mEngine.GetMap().SetTileset(tileset);
}

void Editor::DrawLightingEditor() {
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Beleuchtung");
        ImGui::TextDisabled("Deaktiviert während Playtest - Lighting läuft via Tageszeit + Script System");
        ImGui::Text("Zeit: %.1f h", Lighting::Get().GetTimeOfDay());
        ImGui::End();
        return;
    }
    ImGui::Begin("Beleuchtung");
    auto& lighting = Lighting::Get();
    auto& dir = lighting.GetDirectionalLight();
    auto& amb = lighting.GetAmbient();

    ImGui::Text("Presets");
    if (ImGui::Button("Studio")) lighting.ApplyPreset("studio");
    ImGui::SameLine();
    if (ImGui::Button("Mittag")) lighting.ApplyPreset("noon");
    ImGui::SameLine();
    if (ImGui::Button("Abend")) lighting.ApplyPreset("sunset");
    ImGui::SameLine();
    if (ImGui::Button("Nacht")) lighting.ApplyPreset("night");
    ImGui::SameLine();
    if (ImGui::Button("Bewölkt")) lighting.ApplyPreset("overcast");

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Richtungslicht (Sonne)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Aktiv", &dir.enabled);
        ImGui::DragFloat3("Richtung", &dir.direction.x, 0.01f);
        ImGui::ColorEdit3("Farbe", &dir.color.x);
        ImGui::SliderFloat("Intensität", &dir.intensity, 0.0f, 5.0f);
        ImGui::Checkbox("Schatten werfen", &dir.castShadows);
        ImGui::SliderFloat("Schatten Stärke", &dir.shadowStrength, 0.0f, 1.0f);
    }

    // Schatten System
    if (ImGui::CollapsingHeader("Schatten System", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& shadows = lighting.GetShadows();
        auto& renderer = mEngine.GetRenderer();
        bool shadowsEnabled = renderer.IsShadowsEnabled();
        if (ImGui::Checkbox("Schatten aktiviert", &shadowsEnabled)) {
            renderer.SetShadowsEnabled(shadowsEnabled);
        }
        ImGui::Checkbox("Global Schatten", &shadows.enabled);
        ImGui::SliderInt("Shadow Map Größe", &shadows.mapSize, 512, 4096);
        if (shadows.mapSize != renderer.GetShadowMapTexture() && ImGui::Button("Apply Map Größe (Neustart nötig)")) {
            renderer.InitShadowSystem(shadows.mapSize);
        }
        ImGui::SliderFloat("Bias", &shadows.bias, 0.0001f, 0.02f, "%.4f");
        ImGui::SliderFloat("Stärke", &shadows.strength, 0.0f, 1.0f);
        ImGui::Checkbox("PCF (weiche Schatten)", &shadows.pcf);
        ImGui::SliderFloat("Ortho Größe", &shadows.orthoSize, 5.0f, 100.0f);
        ImGui::SliderFloat("Near", &shadows.nearPlane, 0.1f, 10.0f);
        ImGui::SliderFloat("Far", &shadows.farPlane, 10.0f, 200.0f);
        ImGui::Text("Shadow Map: %dx%d", shadows.mapSize, shadows.mapSize);
        ImGui::TextWrapped("Hinweis: Directional Schatten mit 5x5 PCF + PolygonOffset + Normal Bias – Fix für 'Licht muss auf Ecken achten, nicht drauf leuchten'.");
        ImGui::Separator();
        ImGui::TextWrapped("Cubemap Schatten für Punktlichter jetzt verfügbar (max 2). Siehe Punktlichter Sektion.");
        if (mEngine.GetRenderer().IsPointShadowsEnabled()) {
            ImGui::Text("Aktive Point Shadow Cubemaps: %d", mEngine.GetRenderer().GetPointShadowCount());
        }
    }

    if (ImGui::CollapsingHeader("Umgebungslicht", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Farbe", &amb.color.x);
        ImGui::SliderFloat("Intensität", &amb.intensity, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Tageszeit")) {
        float tod = lighting.GetTimeOfDay();
        if (ImGui::SliderFloat("Uhrzeit", &tod, 0.0f, 24.0f, "%.1f h")) {
            lighting.SetTimeOfDay(tod);
        }
        ImGui::SliderFloat("Geschwindigkeit", &mTimeOfDaySpeed, 0.0f, 10.0f);
        ImGui::TextWrapped("Geschwindigkeit > 0 animiert die Sonne im Editor.");
        mTimeOfDay = tod;
    }

    if (ImGui::CollapsingHeader("Punktlichter (inkl. Cubemap Schatten)")) {
        ImGui::Text("Anzahl: %d", (int)lighting.GetPointLightCount());
        if (ImGui::Button("Punktlicht hinzufügen")) {
            auto& pl = lighting.AddPointLight();
            pl.position = Vec3(0, 3, 0);
            pl.castShadows = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Alle löschen")) lighting.ClearPointLights();
        ImGui::SameLine();
        bool pointShadows = mEngine.GetRenderer().IsPointShadowsEnabled();
        if (ImGui::Checkbox("Cubemap Schatten aktiv", &pointShadows)) {
            mEngine.GetRenderer().SetPointShadowsEnabled(pointShadows);
        }
        ImGui::TextDisabled("Max 2 Punktlichter mit Schatten gleichzeitig (Performance) – Cubemap Support!");
        for (size_t i = 0; i < lighting.GetPointLightCount(); ++i) {
            auto& pl = lighting.GetPointLight(i);
            ImGui::PushID(static_cast<int>(i));
            ImGui::Separator();
            ImGui::Text("Light %zu", i);
            ImGui::Checkbox("An", &pl.enabled);
            ImGui::SameLine();
            ImGui::Checkbox("Schatten (Cubemap)", &pl.castShadows);
            ImGui::DragFloat3("Pos", &pl.position.x, 0.1f);
            ImGui::ColorEdit3("Col", &pl.color.x);
            ImGui::SliderFloat("Int", &pl.intensity, 0.0f, 5.0f);
            ImGui::SliderFloat("Range", &pl.range, 0.5f, 50.0f);
            ImGui::SliderFloat("Shadow Bias", &pl.shadowBias, 0.001f, 0.1f, "%.3f");
            if (pl.castShadows) {
                ImGui::TextColored(ImVec4(1,0.8f,0.3f,1), "Wirft Cubemap-Schatten – Fix für 'Andere Objekte machen kein Schatten'");
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Spotlight")) {
        auto& spot = lighting.GetSpotLight();
        ImGui::Checkbox("Aktiv##spot", &spot.enabled);
        ImGui::DragFloat3("Position##s", &spot.position.x, 0.1f);
        ImGui::DragFloat3("Richtung##s", &spot.direction.x, 0.01f);
        ImGui::ColorEdit3("Farbe##s", &spot.color.x);
        ImGui::SliderFloat("Intensität##s", &spot.intensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Range##s", &spot.range, 0.5f, 80.0f);
        ImGui::SliderFloat("Inner Cone", &spot.innerConeDeg, 1.0f, 80.0f);
        ImGui::SliderFloat("Outer Cone", &spot.outerConeDeg, 1.0f, 90.0f);
    }

    ImGui::End();
}

void Editor::DrawEnvironmentEditor() {
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Umgebung");
        ImGui::TextDisabled("Deaktiviert während Playtest");
        ImGui::End();
        return;
    }
    ImGui::Begin("Umgebung");
    
    auto& renderer = mEngine.GetRenderer();
    auto& fog = renderer.GetFog();
    
    // Fog settings
    if (ImGui::CollapsingHeader("Nebel", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Nebel aktivieren", &fog.enabled);
        ImGui::ColorEdit3("Nebelfarbe", &fog.color.x);
        ImGui::DragFloat("Start-Distanz", &fog.start, 0.5f, 0.0f, fog.end - 1.0f);
        ImGui::DragFloat("End-Distanz", &fog.end, 0.5f, fog.start + 1.0f, 500.0f);
        ImGui::DragFloat("Dichte (Exp)", &fog.density, 0.001f, 0.0f, 0.1f);
        
        if (ImGui::Button("Voreinstellung: Leichter Nebel")) {
            fog.enabled = true;
            fog.color = Color(0.7f, 0.75f, 0.8f, 1.0f);
            fog.start = 20.0f;
            fog.end = 150.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Voreinstellung: Dichter Nebel")) {
            fog.enabled = true;
            fog.color = Color(0.4f, 0.4f, 0.45f, 1.0f);
            fog.start = 5.0f;
            fog.end = 50.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Nebel deaktivieren")) {
            fog.enabled = false;
        }
    }
    
    ImGui::Separator();
    
    // Skybox settings
    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& skybox = renderer.GetSkybox();
        
        if (skybox.IsLoaded()) {
            ImGui::Text("Skybox: Geladen");
            ImGui::DragFloat3("Rotation", &skybox.GetRotation().x, 0.5f);
            if (ImGui::Button("Skybox entladen")) {
                // Reset skybox - would need a method to clear it
            }
        } else {
            ImGui::Text("Keine Skybox geladen");
            
            static char skyboxPath[256] = "assets/textures/skybox/";
            ImGui::InputText("Ordner-Pfad", skyboxPath, sizeof(skyboxPath));
            ImGui::TextWrapped("Legen Sie 6 Texturen in den Ordner: right.png, left.png, top.png, bottom.png, front.png, back.png");
            
            if (ImGui::Button("Skybox laden")) {
                std::array<std::string, 6> faces = {
                    std::string(skyboxPath) + "right.png",
                    std::string(skyboxPath) + "left.png",
                    std::string(skyboxPath) + "top.png",
                    std::string(skyboxPath) + "bottom.png",
                    std::string(skyboxPath) + "front.png",
                    std::string(skyboxPath) + "back.png"
                };
                if (skybox.Load(faces)) {
                    RPG_LOG_INFO("Skybox erfolgreich geladen");
                } else {
                    RPG_LOG_ERROR("Skybox laden fehlgeschlagen");
                }
            }
        }
    }
    
    ImGui::Separator();
    
    // Time of Day / Light cycle (future feature)
    if (ImGui::CollapsingHeader("Tageszeit (Experimental)")) {
        ImGui::TextWrapped("Automatische Sonnenposition und Farbzyklen basierend auf der Zeit.");
        ImGui::SliderFloat("Zeit-Geschwindigkeit", &mTimeOfDaySpeed, 0.0f, 10.0f);
        ImGui::DragFloat("Aktuelle Zeit", &mTimeOfDay, 0.01f, 0.0f, 24.0f);
    }
    
    ImGui::End();
}

void Editor::DrawPrefabBrowser() {
    if (mEngine.IsPlaying()) {
        ImGui::Begin("Prefab Browser");
        ImGui::TextDisabled("Deaktiviert während Playtest");
        ImGui::End();
        return;
    }
    ImGui::Begin("Prefab Browser");

    std::string dir = Prefab::GetPrefabDirectory();
    try {
        fs::create_directories(dir);
        for (const auto& entry : fs::directory_iterator(dir)) {
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
                        auto* material = scene.AddComponent<MaterialComponent>(id);
                        material->material.diffuse = data.color;
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

// ==================== File Dialogs ====================

std::string Editor::OpenFileDialog(const char* filter) {
#if defined(_WIN32)
    OPENFILENAMEA ofn = {};
    char fileName[MAX_PATH] = "";
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Datei öffnen";

    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return "";
#else
    // Linux: Use zenity or kdialog
    std::string cmd = "zenity --file-selection --title=\"Datei öffnen\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

std::string Editor::SaveFileDialog(const char* filter) {
#if defined(_WIN32)
    OPENFILENAMEA ofn = {};
    char fileName[MAX_PATH] = "";
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = "Datei speichern";

    if (GetSaveFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return "";
#else
    std::string cmd = "zenity --file-selection --save --title=\"Datei speichern\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

std::string Editor::SelectFolderDialog() {
#if defined(_WIN32)
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Ordner auswählen";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
#else
    std::string cmd = "zenity --file-selection --directory --title=\"Ordner auswählen\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

// ==================== Gizmo System ====================

void Editor::DrawToolbar() {
    // Gizmo toolbar is drawn as an overlay inside DrawSceneView.
}


void Editor::DrawGizmo() {
    if (mSelectedEntity < 0 || mGizmoMode == GizmoMode::None) return;
    if (mEngine.IsPlaying()) return;
    // Keep drawing even if mouse left the view briefly while dragging
    if (!mSceneViewFocused && !mGizmoActive) return;

    auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
    if (!transform) return;

    Camera& cam = mEngine.GetRenderer().GetCamera();
    Mat4 view = cam.GetViewMatrix();
    Mat4 proj = cam.GetProjectionMatrix();

    Vec3 position = transform->transform.position;
    Vec3 rotation = transform->transform.rotation;
    Vec3 scale = transform->transform.scale;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Project world -> scene-view screen using stored scene rect
    auto worldToScreen = [&](const Vec3& world) -> ImVec2 {
        Vec4 clip = proj * view * Vec4(world, 1.0f);
        if (clip.w <= 0.001f) return ImVec2(-10000, -10000);
        Vec3 ndc(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
        if (ndc.z < -1.0f || ndc.z > 1.0f) return ImVec2(-10000, -10000);
        float x = (ndc.x * 0.5f + 0.5f) * mSceneViewSize.x + mSceneViewPos.x;
        float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * mSceneViewSize.y + mSceneViewPos.y;
        return ImVec2(x, y);
    };

    // Scale axis length with distance so gizmo stays usable
    float dist = glm::length(cam.GetPosition() - position);
    const float axisLen = glm::clamp(dist * 0.18f, 1.4f, 6.0f);
    Vec3 origin = position;

    // Local axes if needed
    Vec3 ax(1,0,0), ay(0,1,0), az(0,0,1);
    if (mGizmoSpace == GizmoSpace::Local) {
        Mat4 rotM = glm::rotate(Mat4(1.0f), glm::radians(rotation.y), Vec3(0,1,0));
        rotM = glm::rotate(rotM, glm::radians(rotation.x), Vec3(1,0,0));
        rotM = glm::rotate(rotM, glm::radians(rotation.z), Vec3(0,0,1));
        ax = glm::normalize(Vec3(rotM * Vec4(1,0,0,0)));
        ay = glm::normalize(Vec3(rotM * Vec4(0,1,0,0)));
        az = glm::normalize(Vec3(rotM * Vec4(0,0,1,0)));
    }

    Vec3 ends[3] = { origin + ax * axisLen, origin + ay * axisLen, origin + az * axisLen };
    ImU32 cols[3] = { IM_COL32(230, 70, 70, 255), IM_COL32(70, 210, 70, 255), IM_COL32(70, 130, 255, 255) };
    ImU32 colsHot[3] = { IM_COL32(255, 220, 60, 255), IM_COL32(255, 220, 60, 255), IM_COL32(255, 220, 60, 255) };
    const char* labels[3] = { "X", "Y", "Z" };

    ImVec2 o = worldToScreen(origin);
    ImVec2 se[3] = { worldToScreen(ends[0]), worldToScreen(ends[1]), worldToScreen(ends[2]) };

    // Draw axes + arrow heads
    for (int i = 0; i < 3; ++i) {
        if (o.x < -9000 || se[i].x < -9000) continue;
        ImU32 col = (mGizmoAxis == i) ? colsHot[i] : cols[i];
        drawList->AddLine(o, se[i], col, (mGizmoAxis == i) ? 7.0f : 5.0f);
        // arrow tip
        ImVec2 dir(se[i].x - o.x, se[i].y - o.y);
        float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
        if (len > 1.0f) {
            dir.x /= len; dir.y /= len;
            ImVec2 n(-dir.y, dir.x);
            ImVec2 p1(se[i].x - dir.x * 16.0f + n.x * 8.0f, se[i].y - dir.y * 16.0f + n.y * 8.0f);
            ImVec2 p2(se[i].x - dir.x * 16.0f - n.x * 8.0f, se[i].y - dir.y * 16.0f - n.y * 8.0f);
            drawList->AddTriangleFilled(se[i], p1, p2, col);
        }
        drawList->AddText(ImVec2(se[i].x + 6, se[i].y - 6), col, labels[i]);
    }
    // center handle
    if (o.x > -9000) drawList->AddCircleFilled(o, 8.0f, IM_COL32(255, 255, 255, 230));
    drawList->AddCircle(o, 10.0f, IM_COL32(0, 0, 0, 180), 0, 2.0f);

    // Interaction
    ImVec2 mousePos = ImGui::GetMousePos();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mSceneViewFocused) {
        float bestDist = 28.0f; // large screen hit tolerance for easier axis grabs
        int bestAxis = -1;
        for (int i = 0; i < 3; ++i) {
            if (se[i].x < -9000) continue;
            // distance point-to-segment in screen space
            ImVec2 a = o, b = se[i];
            ImVec2 ab(b.x - a.x, b.y - a.y);
            ImVec2 ap(mousePos.x - a.x, mousePos.y - a.y);
            float ab2 = ab.x*ab.x + ab.y*ab.y;
            float t = ab2 > 0 ? (ap.x*ab.x + ap.y*ab.y) / ab2 : 0.0f;
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            ImVec2 closest(a.x + ab.x * t, a.y + ab.y * t);
            float d = std::sqrt((mousePos.x - closest.x)*(mousePos.x - closest.x) + (mousePos.y - closest.y)*(mousePos.y - closest.y));
            if (d < bestDist) { bestDist = d; bestAxis = i; }
        }
        if (bestAxis >= 0) {
            mGizmoActive = true;
            mGizmoAxis = bestAxis;
            mGizmoStartPos = position;
            mGizmoStartRot = rotation;
            mGizmoStartScale = scale;
            mGizmoStartMousePos = Vec2(mousePos.x, mousePos.y);
        }
    }

    if (mGizmoActive && ImGui::IsMouseDown(ImGuiMouseButton_Left) && mGizmoAxis >= 0) {
        Vec2 delta(mousePos.x - mGizmoStartMousePos.x, mousePos.y - mGizmoStartMousePos.y);
        Vec3 axisDir = (mGizmoAxis == 0) ? ax : (mGizmoAxis == 1) ? ay : az;

        switch (mGizmoMode) {
            case GizmoMode::Translate: {
                // Project mouse delta onto axis screen direction for natural dragging
                ImVec2 aScr = worldToScreen(mGizmoStartPos);
                ImVec2 bScr = worldToScreen(mGizmoStartPos + axisDir);
                ImVec2 axisScr(bScr.x - aScr.x, bScr.y - aScr.y);
                float axisScrLen = std::sqrt(axisScr.x*axisScr.x + axisScr.y*axisScr.y);
                float along = 0.0f;
                if (axisScrLen > 1.0f) {
                    along = (delta.x * axisScr.x + delta.y * axisScr.y) / (axisScrLen * axisScrLen);
                }
                float move = along * axisLen;
                if (mGizmoSnap) {
                    float s = std::max(0.05f, mGizmoSnapValue);
                    move = std::round(move / s) * s;
                }
                transform->transform.position = mGizmoStartPos + axisDir * move;
                break;
            }
            case GizmoMode::Rotate: {
                float rotDelta = delta.x * 0.4f;
                if (mGizmoSnap) {
                    float s = 15.0f;
                    rotDelta = std::round(rotDelta / s) * s;
                }
                transform->transform.rotation = mGizmoStartRot;
                if (mGizmoAxis == 0) transform->transform.rotation.x = mGizmoStartRot.x + rotDelta;
                else if (mGizmoAxis == 1) transform->transform.rotation.y = mGizmoStartRot.y + rotDelta;
                else transform->transform.rotation.z = mGizmoStartRot.z + rotDelta;
                break;
            }
            case GizmoMode::Scale: {
                float scaleDelta = 1.0f + (delta.x + delta.y) * 0.005f;
                if (scaleDelta < 0.05f) scaleDelta = 0.05f;
                transform->transform.scale = mGizmoStartScale;
                transform->transform.scale[mGizmoAxis] = std::max(0.05f, mGizmoStartScale[mGizmoAxis] * scaleDelta);
                if (mGizmoSnap) {
                    float s = std::max(0.05f, mGizmoSnapValue);
                    transform->transform.scale[mGizmoAxis] = std::round(transform->transform.scale[mGizmoAxis] / s) * s;
                }
                break;
            }
            default: break;
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        mGizmoActive = false;
        mGizmoAxis = -1;
    }
}


void Editor::DrawStatusBar() {
    // Compact debug/status panel (dockable with Konsole)
    ImGui::Begin("Debug / Status");
    ImGui::Text("FPS: %d", mEngine.GetFPS());
    ImGui::SameLine();
    ImGui::Text("| Entities: %zu", mEngine.GetScene().GetEntities().size());
    ImGui::SameLine();
    ImGui::Text("| Map: %dx%d", mEngine.GetMap().GetWidth(), mEngine.GetMap().GetHeight());
    ImGui::Separator();
    ImGui::Text("Gizmo: %s (%s)  Snap: %s",
        mGizmoMode == GizmoMode::None ? "Select" :
        mGizmoMode == GizmoMode::Translate ? "Move" :
        mGizmoMode == GizmoMode::Rotate ? "Rotate" : "Scale",
        mGizmoSpace == GizmoSpace::Local ? "Local" : "World",
        mGizmoSnap ? "ON" : "OFF");
    if (mSelectedEntity >= 0) {
        auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
        if (transform) {
            ImGui::Text("Selected #%d  Pos: %.2f, %.2f, %.2f",
                mSelectedEntity,
                transform->transform.position.x,
                transform->transform.position.y,
                transform->transform.position.z);
        }
    } else {
        ImGui::TextDisabled("No entity selected — click an object in Scene");
    }
    ImGui::Text("Grid: %s | Wireframe: %s | Play: %s",
        mEngine.IsGridVisible() ? "ON" : "OFF",
        mEngine.GetRenderer().IsWireframeEnabled() ? "ON" : "OFF",
        mEngine.IsPlaying() ? "YES" : "no");
    ImGui::End();
}


void Editor::ShowCrashDialog() {
    if (!mShowCrashDialog) return;
    
    ImGui::OpenPopup("CrashDialog");
    if (ImGui::BeginPopupModal("CrashDialog", &mShowCrashDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("APPLICATION CRASHED");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        ImGui::TextWrapped("An unexpected error occurred:");
        ImGui::TextWrapped("%s", mLastCrashInfo.message.c_str());
        
        if (!mLastCrashInfo.stackTrace.empty()) {
            ImGui::Separator();
            ImGui::Text("Stack Trace:");
            ImGui::BeginChild("StackTrace", ImVec2(500, 200), true);
            ImGui::TextWrapped("%s", mLastCrashInfo.stackTrace.c_str());
            ImGui::EndChild();
        }
        
        ImGui::Separator();
        ImGui::Text("What would you like to do?");
        
        if (ImGui::Button("Restart Application", ImVec2(200, 0))) {
            mShowCrashDialog = false;
            // Request restart - in a real app this would trigger a proper restart
            mEngine.RequestQuit();
            // Note: Actual restart would need external launcher
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(200, 0))) {
            mShowCrashDialog = false;
            mEngine.RequestQuit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Continue (Unsafe)", ImVec2(200, 0))) {
            mShowCrashDialog = false;
            // Try to continue - risky but user choice
        }
        
        ImGui::EndPopup();
    }
}


void Editor::HandleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();

    // Play/Stop immer erlauben, auch im Playtest
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        bool isPlaying = mEngine.IsPlaying();
        mEngine.SetPlaying(!isPlaying);
        return; // Im Playtest keine weiteren Shortcuts
    }

    // Alle Edit-Shortcuts blockieren während Playtest
    if (mEngine.IsPlaying()) return;
    
    // Undo/Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (mEngine.GetCommandHistory().CanUndo()) {
            mEngine.GetCommandHistory().Undo(mEngine);
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        if (mEngine.GetCommandHistory().CanRedo()) {
            mEngine.GetCommandHistory().Redo(mEngine);
        }
    }
    
    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput) {
        DeleteSelectedEntity();
    }
    
    // Focus entity
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        if (mSelectedEntity >= 0) {
            auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
            if (transform) {
                Camera& cam = mEngine.GetRenderer().GetCamera();
                Vec3 target = transform->transform.position;
                cam.SetPosition(target + Vec3(0, 3, 5));
                cam.SetRotation(Vec3(-30, 0, 0));
            }
        }
    }
    
    // Gizmo mode shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) mGizmoMode = GizmoMode::None;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false) && !io.WantTextInput) mGizmoMode = GizmoMode::Translate;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false) && !io.WantTextInput) mGizmoMode = GizmoMode::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.WantTextInput) mGizmoMode = GizmoMode::Scale;
    
    // Toggle space
    if (ImGui::IsKeyPressed(ImGuiKey_X, false) && !io.WantTextInput) {
        mGizmoSpace = (mGizmoSpace == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
    }
    // Toggle grid
    if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !io.WantTextInput && !io.KeyCtrl) {
        mEngine.ToggleGrid();
    }
    // Duplicate
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && mSelectedEntity >= 0) {
        EntityID srcId = static_cast<EntityID>(mSelectedEntity);
        auto* st = mEngine.GetScene().GetComponent<TransformComponent>(srcId);
        EntityID id = mEngine.GetScene().CreateEntity(mEngine.GetScene().GetEntityName(srcId) + " Copy");
        auto* t = mEngine.GetScene().AddComponent<TransformComponent>(id);
        if (st) { t->transform = st->transform; t->transform.position.x += 1.0f; }
        if (auto* sm = mEngine.GetScene().GetComponent<ModelRendererComponent>(srcId)) {
            auto* m = mEngine.GetScene().AddComponent<ModelRendererComponent>(id);
            m->model = sm->model; m->texture = sm->texture;
        }
        if (auto* mat = mEngine.GetScene().GetComponent<MaterialComponent>(srcId)) {
            auto* m = mEngine.GetScene().AddComponent<MaterialComponent>(id);
            m->material = mat->material;
        }
        mSelectedEntity = static_cast<int>(id);
    }
}

void Editor::UpdateGizmo() {
    // Called from DrawGizmo
}

void Editor::DrawGizmoAxis(const Vec3& position, const Mat4& view, const Mat4& proj, const Vec2& viewPos, const Vec2& viewSize) {
    // Implementation in DrawGizmo
}

bool Editor::GizmoIntersect(const Vec2& mousePos, const Vec2& viewPos, const Vec2& viewSize, Vec3& outAxis) {
    // Raycast against gizmo axes
    return false;
}

} // namespace rpg