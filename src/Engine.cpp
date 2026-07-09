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

    mWindow = std::make_unique<Window>();
    if (!mWindow->Create(title, width, height, editorMode)) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }

    mRenderer = std::make_unique<Renderer>();
    if (!mRenderer->Initialize()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }

    mInput = std::make_unique<Input>();
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
    mMap->SetTileset(std::make_shared<Tileset>());

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

    mRunning = true;
    return true;
}

void Engine::Shutdown() {
    if (mEditor) {
        mEditor->Shutdown();
        mEditor.reset();
    }
    mResources.reset();
    mMap.reset();
    mProject.reset();
    mScene.reset();
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

        mWindow->PollEvents();
        if (mWindow->ShouldClose()) {
            mRunning = false;
            break;
        }

        Update(dt);
        Render();

        mWindow->SwapBuffers();
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
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // Mapping nur für Esc als Beispiel
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    mInput->OnKeyChanged(Key::Escape, e.type == SDL_KEYDOWN);
                }
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
    }

    mScene->Update(dt);
}

void Engine::Render() {
    mRenderer->BeginFrame(mRenderer->GetCamera());

    // Grid zeichnen
    Mesh grid = MeshFactory::CreateGrid(40, 1.0f);
    mRenderer->DrawMesh(grid, Mat4(1.0f), nullptr, Color(0.4f, 0.4f, 0.4f, 1.0f));

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

    if (mEditor) {
        mEditor->BeginFrame();
        mEditor->DrawUI();
        mEditor->EndFrame();
    }
}

} // namespace rpg
