#include "rpgmaker3d/Scene.h"
#include <algorithm>

namespace rpg {

Scene::Scene() = default;

Scene::~Scene() {
    Clear();
}

EntityID Scene::CreateEntity(const std::string& name) {
    EntityID id = mNextID++;
    mEntities.push_back(id);
    mNames[id] = name;
    return id;
}

void Scene::DestroyEntity(EntityID id) {
    auto it = std::find(mEntities.begin(), mEntities.end(), id);
    if (it != mEntities.end()) {
        mEntities.erase(it);
        mNames.erase(id);
        mComponents.erase(id);
    }
}

const std::string& Scene::GetEntityName(EntityID id) const {
    auto it = mNames.find(id);
    if (it != mNames.end()) return it->second;
    static std::string empty = "Unknown";
    return empty;
}

void Scene::Update(float dt) {
    (void)dt;
    // Script components würden hier aktualisiert werden.
}

void Scene::Clear() {
    mEntities.clear();
    mNames.clear();
    mComponents.clear();
    mNextID = 1;
}

} // namespace rpg
