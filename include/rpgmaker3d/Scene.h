#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Types.h"

namespace rpg {

class Model;
class Texture;

enum class ComponentType {
    Transform,
    Sprite,
    ModelRenderer,
    Script,
    Camera,
    AudioSource,
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
    Color color{1.0f};
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
