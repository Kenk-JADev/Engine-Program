#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/ResourceManager.h"
#include "rpgmaker3d/Editor.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Framebuffer.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/CommandHistory.h"
#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"
#include "rpgmaker3d/Config.h"
#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/UI.h"

#ifdef RPGMAKER3D_BUILD_EDITOR
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#endif

#include <SDL.h>

#include <glad/gl.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace rpg {

Engine::Engine() = default;
Engine::~Engine() { Shutdown(); }

bool Engine::Initialize(const std::string& title, int width, int height, bool editorMode) {
    mEditorMode = editorMode;

    // Logger
    Logger::Get().SetLogFile("engine.log");
    Logger::Get().SetConsoleOutput(true);
    RPG_LOG_INFO(std::string(EngineConfig::NAME) + " v" + EngineConfig::VERSION + " - Init started [" + RPG_PLATFORM_NAME + "]");

    // Plattform
    Platform::SetDPIAware();
    RPG_LOG_INFO("Working Dir: " + Platform::GetWorkingDirectory());
    RPG_LOG_INFO("Exe Path: " + Platform::GetExecutablePath());
#ifdef _WIN32
    RPG_LOG_INFO("Windows Version: " + Platform::GetWindowsVersion());
#endif

    // Fenster
    mWindow = std::make_unique<Window>();
    if (!mWindow->Create(title, width, height, editorMode)) {
        RPG_LOG_ERROR("Failed to create window");
        return false;
    }

    // Renderer
    mRenderer = std::make_unique<Renderer>();
    if (!mRenderer->Initialize()) {
        RPG_LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    // Framebuffer für Editor Scene View
    mSceneFramebuffer = std::make_unique<Framebuffer>();
    if (!mSceneFramebuffer->Create(1280, 720)) {
        RPG_LOG_ERROR("Failed to create scene framebuffer");
        return false;
    }

    // Core Systeme
    mInput = std::make_unique<Input>();
    mCommandHistory = std::make_unique<CommandHistory>();
    mRubyVM = std::make_unique<RubyVM>();
    mRubyVM->Initialize(this);

    mAudio = std::make_unique<AudioManager>();
    if (!mAudio->Initialize()) {
        RPG_LOG_WARN("Audio initialization failed - continuing without audio");
    }

    mScene = std::make_unique<Scene>();
    mProject = std::make_unique<Project>();
    mMap = std::make_unique<Map>();
    mResources = std::make_unique<ResourceManager>();

#ifdef RPGMAKER3D_BUILD_EDITOR
    // ImGui früh initialisieren – auch für Player UI (GameUI nutzt ImGui)
    // Muss nach Window/GL Context, vor allen UI-Systemen passieren
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Viewports temporär deaktiviert gegen Flickering – kann im Editor wieder aktiviert werden
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigDockingAlwaysTabBar = true;
    // Reduziert Flickern beim Resize
    io.ConfigWindowsResizeFromEdges = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    // Flicker-Reduktion
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImGui_ImplSDL2_InitForOpenGL(mWindow->GetNativeWindow(), mWindow->GetGLContext());
    ImGui_ImplOpenGL3_Init("#version 330");
    mImGuiInitialized = true;
    RPG_LOG_INFO("ImGui initialized");
#endif

    // Datenbank laden / Defaults
    try {
        Database::Get().CreateDefaults();
        RPG_LOG_INFO("Database initialized with defaults");
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Database init failed: ") + e.what());
    }

    // Projekt anlegen / laden
    if (std::filesystem::exists("./SampleProject/project.json")) {
        mProject->Load("./SampleProject");
        RPG_LOG_INFO("Loaded SampleProject");
    } else {
        mProject->New("./SampleProject", "Sample RPG 3D");
        RPG_LOG_INFO("Created new SampleProject");
    }

    // Tileset + Map Setup
    mMap->AddLayer("Ground");
    auto tileset = std::make_shared<Tileset>();
    // Versuche mehrere Pfade
    bool tilesetLoaded = false;
    std::vector<std::string> tryPaths = {
        mProject->GetAssetPath("textures/tileset_demo.png"),
        "./SampleProject/assets/textures/tileset_demo.png",
        "assets/textures/tileset_demo.png",
        "./assets/textures/tileset_demo.png"
    };
    for (auto& p : tryPaths) {
        if (Platform::FileExists(p)) {
            tileset->Load(p, 32, 32);
            tilesetLoaded = true;
            RPG_LOG_INFO("Tileset loaded: " + p);
            break;
        }
    }
    if (!tilesetLoaded) {
        tileset->Load("assets/textures/tileset_demo.png", 32, 32); // wird checker fallback
    }
    mMap->SetTileset(tileset);

    // Demo-Map
    for (int z = 0; z < mMap->GetHeight(); ++z) {
        for (int x = 0; x < mMap->GetWidth(); ++x) {
            int tile = ((x + z) % 8);
            mMap->SetTile(0, x, z, tile);
        }
    }

    // Game System – NICHT hier NewGame aufrufen!
    // Game::NewGame() wird vom Player / Editor Play-Mode explizit gestartet,
    // damit Editor und Player identisch sind.
    EventSystem::Get().Clear();

    // Editor
    if (editorMode) {
#ifdef RPGMAKER3D_BUILD_EDITOR
        mEditor = std::make_unique<Editor>(*this);
        if (!mEditor->Initialize(*mWindow)) {
            RPG_LOG_ERROR("Failed to initialize editor");
            return false;
        }
        RPG_LOG_INFO("Editor initialized");
#else
        RPG_LOG_WARN("Editor mode requested but not compiled in");
#endif
    }

    // Demo Entitäten
    EntityID cube = mScene->CreateEntity("Demo Cube");
    auto* tc = mScene->AddComponent<TransformComponent>(cube);
    tc->transform.position = Vec3(0, 0.5f, 0);
    auto* sc = mScene->AddComponent<ModelRendererComponent>(cube);
    sc->model = std::make_shared<Model>();
    sc->model->AddMesh(MeshFactory::CreateCube(1.0f));
    auto* mat = mScene->AddComponent<MaterialComponent>(cube);
    mat->material.diffuse = Color(0.2f, 0.6f, 1.0f, 1.0f);

    EntityID floor = mScene->CreateEntity("Floor");
    auto* tf = mScene->AddComponent<TransformComponent>(floor);
    tf->transform.position = Vec3(0, 0, 0);
    auto* mf = mScene->AddComponent<ModelRendererComponent>(floor);
    mf->model = std::make_shared<Model>();
    mf->model->AddMesh(MeshFactory::CreatePlane(20.0f));

    // Grid
    mGridMesh = MeshFactory::CreateGrid(40, 1.0f);

    // UI
    GameUI::Get().Title().onNewGame = []() {
        Game::Get().NewGame();
        RPG_LOG_INFO("New Game via Title Screen");
    };
    GameUI::Get().Title().onExit = [this]() {
        this->RequestQuit();
    };

    mRunning = true;
    RPG_LOG_INFO("Engine initialized successfully");
    return true;
}

unsigned int Engine::GetSceneTextureID() const {
    if (mSceneFramebuffer) return mSceneFramebuffer->GetTextureID();
    return 0;
}

void Engine::SetPlaying(bool playing) {
    if (playing == mPlayMode) return;
    
    mPlayMode = playing;

    if (mEditorMode) {
        // Editor Play-Test Mode – mit Scene Snapshot
        if (playing) {
            // PlayMode START – snapshot scene, start new game
            RPG_LOG_INFO("Entering Play Mode (Editor)");
            // Save current scene to temp for restore
            if (mProject) {
                SaveScene(mProject->GetProjectPath() + "/__editor_play_backup.json");
            }
            // Reset game state – identical to Player.exe
            Game::Get().NewGame();
            EventSystem::Get().Clear();
            // Reset player position to start
            Game::Get().Player().SetPosition(Vec3(Database::Get().System().startX, 0, Database::Get().System().startY));
        } else {
            // PlayMode STOP – restore editor scene
            RPG_LOG_INFO("Exiting Play Mode (Editor)");
            if (mProject) {
                std::string backup = mProject->GetProjectPath() + "/__editor_play_backup.json";
                if (std::filesystem::exists(backup)) {
                    LoadScene(backup);
                }
            }
            // Game state clear
            EventSystem::Get().Clear();
        }
    } else {
        // Echter Player-Modus – kein Scene-Snapshot, nur Flag setzen
        RPG_LOG_INFO(std::string("Play Mode ") + (playing ? "ON (Player)" : "OFF (Player)"));
    }
}

void Engine::Shutdown() {
    RPG_LOG_INFO("Engine shutdown started");
#ifdef RPGMAKER3D_BUILD_EDITOR
    if (mEditor) {
        mEditor->Shutdown();
        mEditor.reset();
    }
    // ImGui shutdown (Engine owns ImGui context now)
    if (mImGuiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        mImGuiInitialized = false;
    }
#endif
    mGridMesh.Delete();
    if (mResources) mResources.reset();
    if (mMap) mMap.reset();
    if (mProject) mProject.reset();
    if (mScene) mScene.reset();
    if (mCommandHistory) mCommandHistory.reset();
    if (mRubyVM) mRubyVM.reset();
    if (mAudio) mAudio.reset();
    if (mInput) mInput.reset();
    if (mRenderer) mRenderer.reset();
    if (mSceneFramebuffer) mSceneFramebuffer.reset();
    if (mWindow) mWindow.reset();
    mRunning = false;
    RPG_LOG_INFO("Engine shutdown complete");
}

void Engine::Run() {
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime = Clock::now();

    while (mRunning) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        // Clamp dt für Stabilität (z.B. bei Debugger Pause)
        if (dt > 0.1f) dt = 0.1f;
        mDeltaTime = dt;
        mTime += dt;

        mFrameCount++;
        mFPSTimer += dt;
        if (mFPSTimer >= 0.5f) {
            mFPS = static_cast<int>(mFrameCount / mFPSTimer);
            mFrameCount = 0;
            mFPSTimer = 0.0f;
        }

        Update(dt);
        if (!mRunning) break;

        Render();
        mWindow->SwapBuffers();
    }
}

static Key MapSDLKey(SDL_Scancode code) {
    switch (code) {
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_G: return Key::G;
        case SDL_SCANCODE_H: return Key::H;
        case SDL_SCANCODE_I: return Key::I;
        case SDL_SCANCODE_J: return Key::J;
        case SDL_SCANCODE_K: return Key::K;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_N: return Key::N;
        case SDL_SCANCODE_O: return Key::O;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_R: return Key::R;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_U: return Key::U;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_X: return Key::X;
        case SDL_SCANCODE_Y: return Key::Y;
        case SDL_SCANCODE_Z: return Key::Z;
        case SDL_SCANCODE_0: return Key::Num0;
        case SDL_SCANCODE_1: return Key::Num1;
        case SDL_SCANCODE_2: return Key::Num2;
        case SDL_SCANCODE_3: return Key::Num3;
        case SDL_SCANCODE_4: return Key::Num4;
        case SDL_SCANCODE_5: return Key::Num5;
        case SDL_SCANCODE_6: return Key::Num6;
        case SDL_SCANCODE_7: return Key::Num7;
        case SDL_SCANCODE_8: return Key::Num8;
        case SDL_SCANCODE_9: return Key::Num9;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        case SDL_SCANCODE_SPACE: return Key::Space;
        case SDL_SCANCODE_RETURN: return Key::Enter;
        case SDL_SCANCODE_TAB: return Key::Tab;
        case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
        case SDL_SCANCODE_DELETE: return Key::Delete;
        case SDL_SCANCODE_LEFT: return Key::Left;
        case SDL_SCANCODE_RIGHT: return Key::Right;
        case SDL_SCANCODE_UP: return Key::Up;
        case SDL_SCANCODE_DOWN: return Key::Down;
        case SDL_SCANCODE_LSHIFT: return Key::LShift;
        case SDL_SCANCODE_LCTRL: return Key::LCtrl;
        case SDL_SCANCODE_LALT: return Key::LAlt;
        case SDL_SCANCODE_F1: return Key::F1;
        case SDL_SCANCODE_F2: return Key::F2;
        case SDL_SCANCODE_F3: return Key::F3;
        case SDL_SCANCODE_F4: return Key::F4;
        case SDL_SCANCODE_F5: return Key::F5;
        case SDL_SCANCODE_F6: return Key::F6;
        case SDL_SCANCODE_F7: return Key::F7;
        case SDL_SCANCODE_F8: return Key::F8;
        case SDL_SCANCODE_F9: return Key::F9;
        case SDL_SCANCODE_F10: return Key::F10;
        case SDL_SCANCODE_F11: return Key::F11;
        case SDL_SCANCODE_F12: return Key::F12;
        default: return Key::Unknown;
    }
}

void Engine::Update(float dt) {
    mInput->Update();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
#ifdef RPGMAKER3D_BUILD_EDITOR
        if (mImGuiInitialized) ImGui_ImplSDL2_ProcessEvent(&e);
#endif
        switch (e.type) {
            case SDL_QUIT:
                mRunning = false;
                return;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                mInput->OnKeyChanged(MapSDLKey(e.key.keysym.scancode), e.type == SDL_KEYDOWN);
                break;
            case SDL_MOUSEMOTION:
                mInput->OnMouseMoved(static_cast<float>(e.motion.x), static_cast<float>(e.motion.y));
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT)
                    mInput->OnMouseChanged(MouseButton::Left, e.type == SDL_MOUSEBUTTONDOWN);
                if (e.button.button == SDL_BUTTON_RIGHT)
                    mInput->OnMouseChanged(MouseButton::Right, e.type == SDL_MOUSEBUTTONDOWN);
                if (e.button.button == SDL_BUTTON_MIDDLE)
                    mInput->OnMouseChanged(MouseButton::Middle, e.type == SDL_MOUSEBUTTONDOWN);
                break;
            case SDL_MOUSEWHEEL:
                mInput->OnMouseWheel(static_cast<float>(e.wheel.y));
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                    mRunning = false;
                    return;
                }
                break;
        }
    }

    // Global Shortcuts auch außerhalb Editor
    if (mInput->IsKeyPressed(Key::Escape) && !mEditorMode) {
        // Im Spiel: Pause Menü
        if (GameUI::Get().Pause().IsVisible()) GameUI::Get().Pause().Hide();
        else GameUI::Get().Pause().Show();
    }

    // Kamera Navigation (Editor oder Play)
    bool allowCamera = true;
#ifdef RPGMAKER3D_BUILD_EDITOR
    if (mImGuiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            // Im Editor: Kamera nur wenn SceneView fokussiert
            if (mEditor) {
                allowCamera = mEditor->IsSceneViewFocused();
            } else {
                // Im Player: UI blockiert Kamera
                allowCamera = false;
            }
        }
    }
    if (mEditor && mEditor->WantCaptureInput()) allowCamera = false;
#endif

    // Im PlayMode: Kamera folgt optional dem GamePlayer
    // (kann im Editor umgeschaltet werden)
    if (mPlayMode && mPlayModeFollowPlayer) {
        Camera& cam = mRenderer->GetCamera();
        Vec3 playerPos = Game::Get().Player().GetPosition();
        // Simple Follow-Cam: leicht versetzt hinter/über dem Spieler
        Vec3 targetPos = playerPos + Vec3(0, 3.0f, 5.0f);
        cam.SetPosition(targetPos);
        cam.SetRotation(Vec3(-20.0f, 0.0f, 0.0f));
        allowCamera = false; // keine Free-Fly im PlayMode wenn Follow aktiv
    }

    if (allowCamera) {
        Camera& cam = mRenderer->GetCamera();
        float speed = (mInput->IsKeyDown(Key::LShift) ? 10.0f : 5.0f) * dt;
        if (mInput->IsKeyDown(Key::W)) cam.SetPosition(cam.GetPosition() + cam.GetForward() * speed);
        if (mInput->IsKeyDown(Key::S)) cam.SetPosition(cam.GetPosition() - cam.GetForward() * speed);
        if (mInput->IsKeyDown(Key::A)) cam.SetPosition(cam.GetPosition() - cam.GetRight() * speed);
        if (mInput->IsKeyDown(Key::D)) cam.SetPosition(cam.GetPosition() + cam.GetRight() * speed);
        if (mInput->IsKeyDown(Key::Q)) cam.SetPosition(cam.GetPosition() + Vec3(0,1,0) * speed);
        if (mInput->IsKeyDown(Key::E)) cam.SetPosition(cam.GetPosition() - Vec3(0,1,0) * speed);

        if (mInput->IsMouseDown(MouseButton::Right)) {
            Vec2 delta = mInput->GetMouseDelta();
            Vec3 rot = cam.GetRotation();
            rot.y -= delta.x * 0.3f;
            rot.x -= delta.y * 0.3f;
            rot.x = glm::clamp(rot.x, -89.0f, 89.0f);
            cam.SetRotation(rot);
        }

        if (mInput->GetMouseWheel() != 0.0f) {
            cam.SetPosition(cam.GetPosition() + cam.GetForward() * mInput->GetMouseWheel() * 2.0f);
        }
    }

    // Partikel & Scene
    for (EntityID id : mScene->GetEntities()) {
        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter && emitter->emitter) {
            emitter->emitTimer += dt;
            if (emitter->autoEmit && emitter->emitTimer >= emitter->emitRate) {
                auto* transform = mScene->GetComponent<TransformComponent>(id);
                Vec3 origin = transform ? transform->transform.position : Vec3(0.0f);
                emitter->emitter->Emit(emitter->emitCount, origin, emitter->emitDirection,
                    emitter->emitSpread, emitter->emitSpeed, emitter->emitLife, emitter->emitColor);
                emitter->emitTimer = 0.0f;
            }
            emitter->emitter->Update(dt);
        }
    }

    // Game Logic
    if (mPlayMode) {
        Game::Get().Update(dt);
        Game::Get().Player().Update(dt, *mInput);
        EventSystem::Get().Update(dt, Game::Get().Player().GetPosition());
        BattleSystem::Get().Update(dt);
        if (mRubyVM) mRubyVM->Update(dt);
    }

    // UI
    GameUI::Get().Update(dt);

    mScene->Update(dt);
}

void Engine::Render() {
#ifdef RPGMAKER3D_BUILD_EDITOR
    // ImGui New Frame – zentral in Engine
    if (mImGuiInitialized) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    // Editor UI zeichnen (nur Docking etc., kein BeginFrame mehr)
    if (mEditor) {
        mEditor->DrawUI();
    }
#endif

    // Scene in Framebuffer rendern wenn Editor aktiv
#ifdef RPGMAKER3D_BUILD_EDITOR
    if (mEditorMode && mSceneFramebuffer && mEditor) {
        Vec2 viewSize = mEditor->GetSceneViewSize();
        if (viewSize.x > 1 && viewSize.y > 1) {
            mSceneFramebuffer->Resize(static_cast<int>(viewSize.x), static_cast<int>(viewSize.y));
            mSceneFramebuffer->Bind();
            Camera& cam = mRenderer->GetCamera();
            cam.SetPerspective(60.0f, viewSize.x / viewSize.y, 0.1f, 1000.0f);
            RenderScene();
            mSceneFramebuffer->Unbind();
            // Viewport restore – critical for ImGui flicker fix
            if (mWindow) {
                glViewport(0, 0, mWindow->GetWidth(), mWindow->GetHeight());
            }
        }
    } else {
        // Player Modus: direkt rendern
        if (mWindow) {
            Camera& cam = mRenderer->GetCamera();
            cam.SetPerspective(60.0f, (float)mWindow->GetWidth() / (float)mWindow->GetHeight(), 0.1f, 1000.0f);
        }
        RenderScene();
    }
#else
    if (mWindow) {
        Camera& cam = mRenderer->GetCamera();
        cam.SetPerspective(60.0f, (float)mWindow->GetWidth() / (float)mWindow->GetHeight(), 0.1f, 1000.0f);
    }
    RenderScene();
#endif

#ifdef RPGMAKER3D_BUILD_EDITOR
    // Game UI immer innerhalb ImGui Frame zeichnen
    if (mImGuiInitialized) {
        // Im Editor nur im PlayMode, im Player immer
        if (!mEditor || mPlayMode) {
            GameUI::Get().Draw();
        }
        
        // ImGui Render
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Viewport-Handling für Multi-Viewport – aktuell deaktiviert gegen Flickern
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window* backupCurrentWindow = SDL_GL_GetCurrentWindow();
            SDL_GLContext backupCurrentContext = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backupCurrentWindow, backupCurrentContext);
            // Viewport restore nach Platform Windows
            if (mWindow) {
                glViewport(0, 0, mWindow->GetWidth(), mWindow->GetHeight());
            }
        }
    }
#endif
}

void Engine::RenderScene() {
    // Bestimme aktive Kamera - vermeide goto und dangling pointer (MSVC mag goto nicht)
    Camera activeCam;
    bool hasActiveCam = false;
    Camera* camera = &mRenderer->GetCamera();

    if (mActiveCameraEntity != INVALID_ENTITY) {
        auto* camComp = mScene->GetComponent<CameraComponent>(mActiveCameraEntity);
        auto* transform = mScene->GetComponent<TransformComponent>(mActiveCameraEntity);
        if (camComp && transform) {
            activeCam.SetPosition(transform->transform.position);
            activeCam.SetRotation(transform->transform.rotation);
            activeCam.SetPerspective(camComp->fov, camComp->aspect, camComp->nearPlane, camComp->farPlane);
            camera = &activeCam;
            hasActiveCam = true;
        }
    }

    if (hasActiveCam) {
        mRenderer->BeginFrame(activeCam);
    } else {
        mRenderer->BeginFrame(*camera);
    }

    // Grid (nur im Editor)
    if (mEditorMode) {
        mRenderer->DrawMesh(mGridMesh, Mat4(1.0f), nullptr, Color(0.4f, 0.4f, 0.4f, 0.6f));
    }

    // Map
    mMap->Render(*mRenderer);

    // Entitäten
    for (EntityID id : mScene->GetEntities()) {
        auto* transform = mScene->GetComponent<TransformComponent>(id);
        if (!transform) continue;

        auto* model = mScene->GetComponent<ModelRendererComponent>(id);
        auto* material = mScene->GetComponent<MaterialComponent>(id);

        if (model && model->model) {
            Mat4 matrix = transform->transform.GetMatrix();
            if (material) {
                if (model->model->GetMeshCount() > 0)
                    mRenderer->DrawMeshWithMaterial(model->model->GetMesh(0), matrix, material->material);
            } else {
                mRenderer->DrawModel(*model->model, matrix, model->texture.get());
            }
        }

        auto* light = mScene->GetComponent<LightComponent>(id);
        if (light) {
            Mesh lightMesh = MeshFactory::CreateCube(0.2f);
            Mat4 matrix = glm::translate(Mat4(1.0f), transform->transform.position);
            mRenderer->DrawMesh(lightMesh, matrix, nullptr, light->color);
        }

        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter && emitter->emitter) {
            mRenderer->DrawParticles(emitter->emitter->GetParticles());
        }

        // Sprite als Billboard
        auto* sprite = mScene->GetComponent<SpriteComponent>(id);
        if (sprite) {
            Mat4 matrix;
            if (sprite->billboard) {
                Vec3 pos = transform->transform.position;
                Vec3 camPos = camera->GetPosition();
                Vec3 forward = glm::normalize(camPos - pos);
                Vec3 right = glm::normalize(glm::cross(Vec3(0,1,0), forward));
                Vec3 up = glm::cross(forward, right);
                Mat4 rot(right.x, right.y, right.z, 0,
                         up.x, up.y, up.z, 0,
                         forward.x, forward.y, forward.z, 0,
                         0,0,0,1);
                matrix = glm::translate(Mat4(1.0f), pos) * rot;
                matrix = glm::scale(matrix, Vec3(sprite->size.x * transform->transform.scale.x,
                                                 sprite->size.y * transform->transform.scale.y, 1.0f));
            } else {
                matrix = transform->transform.GetMatrix();
                matrix = glm::scale(matrix, Vec3(sprite->size.x, sprite->size.y, 1.0f));
            }
            Mesh quad = MeshFactory::CreateQuad(1.0f, 1.0f);
            mRenderer->DrawMesh(quad, matrix, sprite->texture.get(), sprite->color);
        }
    }

    // Auswahl-BoundingBox im Editor
#ifdef RPGMAKER3D_BUILD_EDITOR
    if (mEditorMode && mEditor && !mPlayMode) {
        int selected = mEditor->GetSelectedEntity();
        if (selected >= 0) {
            auto* transform = mScene->GetComponent<TransformComponent>(static_cast<EntityID>(selected));
            if (transform) {
                Mat4 matrix = transform->transform.GetMatrix();
                mRenderer->DrawBoundingBox(Vec3(-0.5f), Vec3(0.5f), matrix, Color(1.0f, 0.8f, 0.0f, 1.0f));
            }
        }
    }
#endif

    // Player Visual im PlayMode – damit Editor Play-Test = Player aussieht
    if (mPlayMode) {
        Vec3 playerPos = Game::Get().Player().GetPosition();
        static Mesh playerMesh;
        static bool playerMeshInit = false;
        if (!playerMeshInit) {
            playerMesh = MeshFactory::CreateCube(0.8f);
            playerMeshInit = true;
        }
        Mat4 playerMat = glm::translate(Mat4(1.0f), playerPos + Vec3(0, 0.4f, 0));
        mRenderer->DrawMesh(playerMesh, playerMat, nullptr, Color(0.2f, 1.0f, 0.3f, 1.0f));
    }

    mRenderer->EndFrame();
}

// Helpers für JSON Escaping
namespace {
std::string EscapeJSON(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}
std::string Vec3ToJSON(const Vec3& v) {
    return "[" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "]";
}
std::string Vec4ToJSON(const Vec4& v) {
    return "[" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w) + "]";
}
}

void Engine::SaveScene(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to save scene: " + path);
        return;
    }
    file << "{\n  \"entities\": [\n";
    const auto& entities = mScene->GetEntities();
    for (size_t i = 0; i < entities.size(); ++i) {
        EntityID id = entities[i];
        file << "    {\n";
        file << "      \"id\": " << id << ",\n";
        file << "      \"name\": \"" << EscapeJSON(mScene->GetEntityName(id)) << "\"";

        auto* transform = mScene->GetComponent<TransformComponent>(id);
        if (transform) {
            file << ",\n      \"transform\": {\n";
            file << "        \"position\": " << Vec3ToJSON(transform->transform.position) << ",\n";
            file << "        \"rotation\": " << Vec3ToJSON(transform->transform.rotation) << ",\n";
            file << "        \"scale\": " << Vec3ToJSON(transform->transform.scale) << "\n";
            file << "      }";
        }

        auto* model = mScene->GetComponent<ModelRendererComponent>(id);
        if (model) {
            file << ",\n      \"model\": {\"mesh\": \"primitive\"}";
        }
        auto* material = mScene->GetComponent<MaterialComponent>(id);
        if (material) {
            file << ",\n      \"material\": {\"diffuse\": " << Vec4ToJSON(material->material.diffuse) << "}";
        }
        auto* light = mScene->GetComponent<LightComponent>(id);
        if (light) {
            file << ",\n      \"light\": {\"color\": " << Vec4ToJSON(light->color) << ", \"intensity\": " << light->intensity << "}";
        }
        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter) {
            file << ",\n      \"particleEmitter\": {\"autoEmit\": " << (emitter->autoEmit ? "true" : "false") << "}";
        }

        file << "\n    }";
        if (i + 1 < entities.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n}\n";
    RPG_LOG_INFO("Scene saved to: " + path);
}

bool Engine::LoadScene(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to load scene: " + path);
        return false;
    }
    mScene->Clear();
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto findSection = [&](const std::string& text, const std::string& key, size_t start) -> size_t {
        return text.find("\"" + key + "\"", start);
    };
    auto parseVec3 = [&](const std::string& text, size_t start, size_t end, Vec3& out) {
        size_t bracket = text.find('[', start);
        if (bracket == std::string::npos || bracket >= end) return;
        size_t close = text.find(']', bracket);
        if (close == std::string::npos || close > end) return;
        std::string inner = text.substr(bracket + 1, close - bracket - 1);
        std::stringstream ss(inner);
        char sep;
        ss >> out.x >> sep >> out.y >> sep >> out.z;
    };
    auto parseVec4 = [&](const std::string& text, size_t start, size_t end, Vec4& out) {
        size_t bracket = text.find('[', start);
        if (bracket == std::string::npos || bracket >= end) return;
        size_t close = text.find(']', bracket);
        if (close == std::string::npos || close > end) return;
        std::string inner = text.substr(bracket + 1, close - bracket - 1);
        std::stringstream ss(inner);
        char sep;
        ss >> out.x >> sep >> out.y >> sep >> out.z >> sep >> out.w;
    };

    size_t entityStart = content.find("\"entities\"");
    if (entityStart == std::string::npos) return false;
    size_t arrayStart = content.find('[', entityStart);
    if (arrayStart == std::string::npos) return false;

    size_t pos = arrayStart + 1;
    while (pos < content.size()) {
        size_t objStart = content.find('{', pos);
        if (objStart == std::string::npos) break;
        // naive find matching }: need depth
        int depth = 0;
        size_t objEnd = std::string::npos;
        for (size_t i = objStart; i < content.size(); ++i) {
            if (content[i] == '{') depth++;
            else if (content[i] == '}') {
                depth--;
                if (depth == 0) { objEnd = i; break; }
            }
        }
        if (objEnd == std::string::npos) break;

        std::string obj = content.substr(objStart, objEnd - objStart + 1);

        size_t namePos = findSection(obj, "name", 0);
        std::string name = "Entity";
        if (namePos != std::string::npos) {
            size_t colon = obj.find(':', namePos);
            size_t q1 = obj.find('\"', colon);
            size_t q2 = obj.find('\"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) name = obj.substr(q1 + 1, q2 - q1 - 1);
        }

        EntityID id = mScene->CreateEntity(name);

        size_t transformPos = findSection(obj, "transform", 0);
        if (transformPos != std::string::npos) {
            auto* t = mScene->AddComponent<TransformComponent>(id);
            size_t pPos = findSection(obj, "position", transformPos);
            parseVec3(obj, pPos, obj.size(), t->transform.position);
            size_t rPos = findSection(obj, "rotation", transformPos);
            parseVec3(obj, rPos, obj.size(), t->transform.rotation);
            size_t sPos = findSection(obj, "scale", transformPos);
            parseVec3(obj, sPos, obj.size(), t->transform.scale);
        }

        if (findSection(obj, "model", 0) != std::string::npos) {
            auto* m = mScene->AddComponent<ModelRendererComponent>(id);
            m->model = std::make_shared<Model>();
            m->model->AddMesh(MeshFactory::CreateCube(1.0f));
        }

        size_t matPos = findSection(obj, "material", 0);
        if (matPos != std::string::npos) {
            auto* m = mScene->AddComponent<MaterialComponent>(id);
            size_t dPos = findSection(obj, "diffuse", matPos);
            parseVec4(obj, dPos, obj.size(), m->material.diffuse);
        }

        size_t lightPos = findSection(obj, "light", 0);
        if (lightPos != std::string::npos) {
            auto* l = mScene->AddComponent<LightComponent>(id);
            size_t cPos = findSection(obj, "color", lightPos);
            parseVec4(obj, cPos, obj.size(), l->color);
        }

        size_t pePos = findSection(obj, "particleEmitter", 0);
        if (pePos != std::string::npos) {
            auto* pe = mScene->AddComponent<ParticleEmitterComponent>(id);
            pe->emitter = std::make_unique<ParticleEmitter>();
            size_t aePos = findSection(obj, "autoEmit", pePos);
            if (aePos != std::string::npos) {
                size_t colon = obj.find(':', aePos);
                if (colon != std::string::npos) {
                    std::string val = obj.substr(colon + 1);
                    pe->autoEmit = (val.find("true") != std::string::npos);
                }
            }
        }

        pos = objEnd + 1;
    }

    RPG_LOG_INFO("Scene loaded from: " + path);
    return true;
}

} // namespace rpg
