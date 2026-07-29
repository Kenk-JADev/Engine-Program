#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Types.h"
#include "Material.h"
#include "Model.h" // PAKET 47: Instanz-Renderpuffer (Mesh) brauchen den vollen Typ

namespace rpg {

class Texture;
class ParticleEmitter;

enum class ComponentType {
    Transform,
    Sprite,
    ModelRenderer,
    Material,
    Script,
    Camera,
    AudioSource,
    ParticleEmitter,
    Light,
    Count
};

class Component {
public:
    virtual ~Component() = default;
    virtual ComponentType GetType() const = 0;
    EntityID owner = INVALID_ENTITY;
};

class TransformComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::Transform; }
    Transform transform;
};

class SpriteComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::Sprite; }
    std::shared_ptr<Texture> texture;
    Color color{1.0f};
    Vec2 size{1.0f, 1.0f};
    bool billboard = true;
};

class ModelRendererComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::ModelRenderer; }
    std::shared_ptr<Model> model;
    std::shared_ptr<Texture> texture;

    // ---- PAKET 47 (Etappe 3, Stufe 2): Instanz-Clip-Laufzeit ----
    // Das Model bleibt die reine Vorlage (Frames+Clips); Clip-Stand und
    // Morph-Puffer liegen JE ENTITAET — mehrere Entitaeten mit demselben
    // Model laufen ab Stufe 2 unabhaengig (behebt die Stufe-1-Grenze
    // "geteilte Pose"). animClip < 0 = keine eigene Pose aktiv.
    int animClip = -1;
    bool animPlaying = false;
    float animTime = 0.0f;
    /// Eigene Renderpuffer (gleiche Mesh-Zahl wie das Model). Leer =
    /// zeichnen ueber die geteilten Template-Meshes (statische Modelle,
    /// keine zusaetzliche GPU-Last).
    std::vector<Mesh> instanceMeshes;

    bool HasInstanceMeshes() const { return !instanceMeshes.empty(); }
    size_t GetDrawMeshCount() const;
    const Mesh& GetDrawMesh(size_t index) const; // guarded wie model-Meshes
};

class MaterialComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::Material; }
    Material material;
};

class LightComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::Light; }
    Color color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
};

class ParticleEmitterComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::ParticleEmitter; }
    std::unique_ptr<ParticleEmitter> emitter;
    bool autoEmit = false;
    int emitCount = 5;
    float emitRate = 0.1f;
    float emitTimer = 0.0f;
    Vec3 emitDirection{0, 1, 0};
    float emitSpread = 0.5f;
    float emitSpeed = 2.0f;
    float emitLife = 1.0f;
    Color emitColor{1.0f, 0.5f, 0.0f, 1.0f};
};

class CameraComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::Camera; }
    float fov = 60.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    bool isMain = true;
};

class ScriptComponent : public Component {
public:
    ComponentType GetType() const override { return ComponentType::Script; }
    std::string scriptPath;
    std::string className;
};

class Scene {
public:
    Scene();
    ~Scene();

    EntityID CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(EntityID id);
    const std::string& GetEntityName(EntityID id) const;
    void SetEntityName(EntityID id, const std::string& name) { mNames[id] = name; }

    // ---- PAKET 47 (Etappe 3, Stufe 2): Instanz-Clip-Steuerung ----
    /// Startet einen Morph-Clip AUF DIESER Entitaet (eigene Pose + Puffer).
    /// false: kein Model / kein Manifest / Clip unbekannt (Log).
    bool StartEntityClip(EntityID id, int clipIndex, bool restart = true);
    bool StartEntityClip(EntityID id, const std::string& clipName,
                         bool restart = true);
    /// Haelt die aktuelle Pose an (Instanz-Puffer bleiben bestehen).
    bool StopEntityClip(EntityID id);

    template<typename T>
    T* AddComponent(EntityID id) {
        auto comp = std::make_unique<T>();
        comp->owner = id;
        T* ptr = comp.get();
        mComponents[id].push_back(std::move(comp));
        return ptr;
    }

    template<typename T>
    T* GetComponent(EntityID id) {
        auto it = mComponents.find(id);
        if (it == mComponents.end()) return nullptr;
        for (auto& c : it->second) {
            if (c->GetType() == T{}.GetType()) return static_cast<T*>(c.get());
        }
        return nullptr;
    }

    void Update(float dt);
    void Clear();

    const std::vector<EntityID>& GetEntities() const { return mEntities; }

private:
    EntityID mNextID = 1;
    std::vector<EntityID> mEntities;
    std::unordered_map<EntityID, std::string> mNames;
    std::unordered_map<EntityID, std::vector<std::unique_ptr<Component>>> mComponents;
};

} // namespace rpg
