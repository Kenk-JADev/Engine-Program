#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include <imgui_impl_sdl2.h>
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

#if defined(_WIN32)
#include <SDL.h>
#else
#include <SDL.h>
#endif

#include <glad/gl.h>
#include <iostream>
#include <chrono>

namespace rpg {

Engine::Engine() = default;

Engine::~Engine() {
    Shutdown();
}

bool Engine::Initialize(const std::string& title, int width, int height, bool editorMode) {
    mEditorMode = editorMode;

    Logger::Get().SetLogFile("engine.log");
    Logger::Get().SetConsoleOutput(true);
    RPG_LOG_INFO("Engine initialization started");

    mWindow = std::make_unique<Window>();
    if (!mWindow->Create(title, width, height, editorMode)) {
        RPG_LOG_ERROR("Failed to create window");
        return false;
    }
    RPG_LOG_INFO("Window created: " + std::to_string(width) + "x" + std::to_string(height));

    mRenderer = std::make_unique<Renderer>();
    if (!mRenderer->Initialize()) {
        RPG_LOG_ERROR("Failed to initialize renderer");
        return false;
    }
    RPG_LOG_INFO("Renderer initialized");

    mSceneFramebuffer = std::make_unique<Framebuffer>();
    if (!mSceneFramebuffer->Create(1280, 720)) {
        RPG_LOG_ERROR("Failed to create scene framebuffer");
        return false;
    }
    RPG_LOG_INFO("Scene framebuffer created");

    mInput = std::make_unique<Input>();
    mCommandHistory = std::make_unique<CommandHistory>();
    mRubyVM = std::make_unique<RubyVM>();
    mRubyVM->Initialize(this);
    mAudio = std::make_unique<AudioManager>();
    if (!mAudio->Initialize()) {
        std::cerr << "Warning: Audio initialization failed" << std::endl;
    }

    mScene = std::make_unique<Scene>();
    mProject = std::make_unique<Project>();
    mMap = std::make_unique<Map>();
    mResources = std::make_unique<ResourceManager>();

    // Standard-Projekt anlegen
    mProject->New("./SampleProject", "Sample RPG");
    mMap->AddLayer("Ground");
    auto tileset = std::make_shared<Tileset>();
    tileset->Load(mProject->GetAssetPath("textures/tileset_demo.png"), 32, 32);
    mMap->SetTileset(tileset);

    // Demo-Map befüllen
    for (int z = 0; z < mMap->GetHeight(); ++z) {
        for (int x = 0; x < mMap->GetWidth(); ++x) {
            int tile = ((x + z) % 8);
            mMap->SetTile(0, x, z, tile);
        }
    }

    if (editorMode) {
        mEditor = std::make_unique<Editor>(*this);
        if (!mEditor->Initialize(*mWindow)) {
            std::cerr << "Failed to initialize editor" << std::endl;
            return false;
        }
    }

    // Demo-Entität
    EntityID cube = mScene->CreateEntity("Demo Cube");
    mScene->AddComponent<TransformComponent>(cube);
    auto* sc = mScene->AddComponent<SpriteComponent>(cube);
    sc->size = Vec2(1.0f, 1.0f);
    (void)sc;

    // Grid einmalig erstellen
    mGridMesh = MeshFactory::CreateGrid(40, 1.0f);

    mRunning = true;
    return true;
}

unsigned int Engine::GetSceneTextureID() const {
    if (mSceneFramebuffer) return mSceneFramebuffer->GetTextureID();
    return 0;
}

void Engine::Shutdown() {
    if (mEditor) {
        mEditor->Shutdown();
        mEditor.reset();
    }
    mGridMesh.Delete();
    mResources.reset();
    mMap.reset();
    mProject.reset();
    mScene.reset();
    mCommandHistory.reset();
    mRubyVM.reset();
    mAudio.reset();
    mInput.reset();
    mRenderer.reset();
    mWindow.reset();
    mRunning = false;
}

void Engine::Run() {
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime = Clock::now();

    while (mRunning) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
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

    // SDL-Events an Input weiterleiten
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (mEditor) ImGui_ImplSDL2_ProcessEvent(&e);

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
                break;
            case SDL_MOUSEWHEEL:
                mInput->OnMouseWheel(static_cast<float>(e.wheel.y));
                break;
        }
    }

    // Editor-Kamera-Navigation
    if (mEditor && !mEditor->WantCaptureInput()) {
        Camera& cam = mRenderer->GetCamera();
        float speed = 5.0f * dt;
        if (mInput->IsKeyDown(Key::W)) cam.SetPosition(cam.GetPosition() + cam.GetForward() * speed);
        if (mInput->IsKeyDown(Key::S)) cam.SetPosition(cam.GetPosition() - cam.GetForward() * speed);
        if (mInput->IsKeyDown(Key::A)) cam.SetPosition(cam.GetPosition() - cam.GetRight() * speed);
        if (mInput->IsKeyDown(Key::D)) cam.SetPosition(cam.GetPosition() + cam.GetRight() * speed);

        // Maus-Look mit Rechtsklick
        if (mInput->IsMouseDown(MouseButton::Right)) {
            Vec2 delta = mInput->GetMouseDelta();
            Vec3 rot = cam.GetRotation();
            rot.y -= delta.x * 0.3f;
            rot.x -= delta.y * 0.3f;
            rot.x = glm::clamp(rot.x, -89.0f, 89.0f);
            cam.SetRotation(rot);
        }

        // Zoom mit Mausrad
        if (mInput->GetMouseWheel() != 0.0f) {
            cam.SetPosition(cam.GetPosition() + cam.GetForward() * mInput->GetMouseWheel() * 2.0f);
        }
    }

    mScene->Update(dt);
}

void Engine::Render() {
    // Editor UI vorbereiten (Berechnet SceneViewSize)
    if (mEditor) {
        mEditor->BeginFrame();
        mEditor->DrawUI();
    }

    // Im Editor in den Scene-Framebuffer rendern
    if (mEditorMode && mSceneFramebuffer && mEditor) {
        Vec2 viewSize = mEditor->GetSceneViewSize();
        if (viewSize.x > 0 && viewSize.y > 0) {
            mSceneFramebuffer->Resize(static_cast<int>(viewSize.x), static_cast<int>(viewSize.y));
            mSceneFramebuffer->Bind();

            Camera& cam = mRenderer->GetCamera();
            cam.SetPerspective(60.0f, viewSize.x / viewSize.y, 0.1f, 1000.0f);

            RenderScene();

            mSceneFramebuffer->Unbind();
        }
    } else {
        RenderScene();
    }

    // Editor UI auf den Bildschirm rendern
    if (mEditor) {
        mEditor->EndFrame();
    }
}

void Engine::RenderScene() {
    mRenderer->BeginFrame(mRenderer->GetCamera());

    // Grid zeichnen (wiederverwendet)
    mRenderer->DrawMesh(mGridMesh, Mat4(1.0f), nullptr, Color(0.4f, 0.4f, 0.4f, 1.0f));

    // Map zeichnen
    mMap->Render(*mRenderer);

    // Entitäten zeichnen
    for (EntityID id : mScene->GetEntities()) {
        auto* transform = mScene->GetComponent<TransformComponent>(id);
        auto* model = mScene->GetComponent<ModelRendererComponent>(id);
        if (transform && model) {
            mRenderer->DrawModel(*model->model, transform->transform.GetMatrix(), model->texture.get());
        }
    }

    mRenderer->EndFrame();
}

} // namespace rpg
