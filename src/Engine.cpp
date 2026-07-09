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
#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"

#if defined(_WIN32)
#include <SDL.h>
#else
#include <SDL.h>
#endif

#include <glad/gl.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>

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

    // Partikel aktualisieren
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

    // Ruby-Update im Play Mode
    if (mPlayMode) {
        mRubyVM->Update(dt);
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
    Camera* camera = &mRenderer->GetCamera();
    if (mActiveCameraEntity != INVALID_ENTITY) {
        auto* camComp = mScene->GetComponent<CameraComponent>(mActiveCameraEntity);
        auto* transform = mScene->GetComponent<TransformComponent>(mActiveCameraEntity);
        if (camComp && transform) {
            Camera tempCam;
            tempCam.SetPosition(transform->transform.position);
            tempCam.SetRotation(transform->transform.rotation);
            tempCam.SetPerspective(camComp->fov, camComp->aspect, camComp->nearPlane, camComp->farPlane);
            camera = &tempCam;
        }
    }

    mRenderer->BeginFrame(*camera);

    // Grid zeichnen (wiederverwendet)
    mRenderer->DrawMesh(mGridMesh, Mat4(1.0f), nullptr, Color(0.4f, 0.4f, 0.4f, 1.0f));

    // Map zeichnen
    mMap->Render(*mRenderer);

    // Entitäten zeichnen
    for (EntityID id : mScene->GetEntities()) {
        auto* transform = mScene->GetComponent<TransformComponent>(id);
        auto* model = mScene->GetComponent<ModelRendererComponent>(id);
        auto* material = mScene->GetComponent<MaterialComponent>(id);
        if (transform && model) {
            Mat4 matrix = transform->transform.GetMatrix();
            if (material) {
                mRenderer->DrawMeshWithMaterial(model->model->GetMesh(0), matrix, material->material);
            } else {
                mRenderer->DrawModel(*model->model, matrix, model->texture.get());
            }
        }

        auto* light = mScene->GetComponent<LightComponent>(id);
        if (transform && light) {
            Mesh lightMesh = MeshFactory::CreateCube(0.2f);
            Mat4 matrix = glm::translate(Mat4(1.0f), transform->transform.position);
            mRenderer->DrawMesh(lightMesh, matrix, nullptr, light->color);
        }

        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter && emitter->emitter) {
            mRenderer->DrawParticles(emitter->emitter->GetParticles());
        }

        auto* sprite = mScene->GetComponent<SpriteComponent>(id);
        if (transform && sprite) {
            Mat4 matrix;
            if (sprite->billboard) {
                // Face camera
                Vec3 pos = transform->transform.position;
                Vec3 camPos = camera->GetPosition();
                Vec3 forward = glm::normalize(camPos - pos);
                Vec3 right = glm::normalize(glm::cross(Vec3(0, 1, 0), forward));
                Vec3 up = glm::cross(forward, right);
                Mat4 rot(right.x, right.y, right.z, 0,
                         up.x, up.y, up.z, 0,
                         forward.x, forward.y, forward.z, 0,
                         0, 0, 0, 1);
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

    // Auswahl-Gizmo
    if (mEditor && mEditorMode) {
        int selected = mEditor->GetSelectedEntity();
        if (selected >= 0) {
            auto* transform = mScene->GetComponent<TransformComponent>(static_cast<EntityID>(selected));
            if (transform) {
                Vec3 scale = transform->transform.scale;
                if (glm::length(scale) < 0.001f) scale = Vec3(1.0f);
                Mat4 matrix = transform->transform.GetMatrix();
                mRenderer->DrawBoundingBox(Vec3(-0.5f), Vec3(0.5f), matrix, Color(1.0f, 0.8f, 0.0f, 1.0f));
            }
        }
    }

    mRenderer->EndFrame();
}

namespace {

std::string EscapeJSON(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
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

} // anonymous namespace

void Engine::SaveScene(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to save scene: " + path);
        return;
    }

    file << "{\n";
    file << "  \"entities\": [\n";
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
            file << ",\n      \"model\": {\n";
            file << "        \"mesh\": \"primitive\"\n";
            file << "      }";
        }

        auto* material = mScene->GetComponent<MaterialComponent>(id);
        if (material) {
            file << ",\n      \"material\": {\n";
            file << "        \"diffuse\": " << Vec4ToJSON(material->material.diffuse) << "\n";
            file << "      }";
        }

        auto* light = mScene->GetComponent<LightComponent>(id);
        if (light) {
            file << ",\n      \"light\": {\n";
            file << "        \"color\": " << Vec4ToJSON(light->color) << ",\n";
            file << "        \"intensity\": " << light->intensity << "\n";
            file << "      }";
        }

        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter) {
            file << ",\n      \"particleEmitter\": {\n";
            file << "        \"autoEmit\": " << (emitter->autoEmit ? "true" : "false") << ",\n";
            file << "        \"color\": " << Vec4ToJSON(emitter->emitColor) << "\n";
            file << "      }";
        }

        file << "\n    }";
        if (i + 1 < entities.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

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
        std::string pattern = "\"" + key + "\"";
        return text.find(pattern, start);
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

        size_t objEnd = content.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = content.substr(objStart, objEnd - objStart + 1);

        size_t namePos = findSection(obj, "name", 0);
        std::string name = "Entity";
        if (namePos != std::string::npos) {
            size_t quote = obj.find('"', namePos + 7);
            size_t quoteEnd = obj.find('"', quote + 1);
            if (quote != std::string::npos && quoteEnd != std::string::npos) {
                name = obj.substr(quote + 1, quoteEnd - quote - 1);
            }
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

        size_t modelPos = findSection(obj, "model", 0);
        if (modelPos != std::string::npos) {
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
            size_t iPos = findSection(obj, "intensity", lightPos);
            if (iPos != std::string::npos) {
                size_t colon = obj.find(':', iPos);
                if (colon != std::string::npos) {
                    std::string val = obj.substr(colon + 1);
                    try { l->intensity = std::stof(val); } catch (...) {}
                }
            }
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
            size_t cPos = findSection(obj, "color", pePos);
            parseVec4(obj, cPos, obj.size(), pe->emitColor);
        }

        pos = objEnd + 1;
        while (pos < content.size() && (content[pos] == ',' || std::isspace(static_cast<unsigned char>(content[pos])))) ++pos;
    }

    RPG_LOG_INFO("Scene loaded from: " + path);
    return true;
}

} // namespace rpg
