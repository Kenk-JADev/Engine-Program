#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Model.h" // PAKET 46/47: Anim-API vollstaendig
#include "rpgmaker3d/Logger.h" // PAKET 47: RPG_LOG_WARN
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

namespace {

// PAKET 47: eigene Renderpuffer je Entitaet anlegen (1:1 zum Template-
// Mesh: Indizes geteilt als CPU-Kopie, eigene dynamische VBOs). Laeuft im
// Engine-Thread (GameLoop), GL-Kontext vorhanden; headless ist BuildGPU
// ohnehin No-Op-sicher.
void EnsureInstanceMeshes(ModelRendererComponent& mr) {
    if (!mr.instanceMeshes.empty() || !mr.model) return;
    const size_t count = mr.model->GetMeshCount();
    mr.instanceMeshes.resize(count);
    for (size_t i = 0; i < count; ++i) {
        Mesh& dst = mr.instanceMeshes[i];
        const Mesh& base = mr.model->GetMesh(i);
        dst.vertices = base.vertices;   // Basis = Frame 0, Morph schreibt daraus
        dst.indices = base.indices;
        dst.textureName = base.textureName;
        dst.dynamicDraw = true;
        dst.BuildGPU();
    }
}

} // namespace

size_t ModelRendererComponent::GetDrawMeshCount() const {
    if (!instanceMeshes.empty()) return instanceMeshes.size();
    return model ? model->GetMeshCount() : 0;
}

const Mesh& ModelRendererComponent::GetDrawMesh(size_t index) const {
    if (!instanceMeshes.empty()) return instanceMeshes[index];
    return model->GetMesh(index);
}

void Scene::Update(float dt) {
    // PAKET 46/47 (Etappe 3): Morph-Animation JE ENTITAET (Stufe 2).
    // Das Model bleibt reine Vorlage; Autostart (start=1) wird hier auf die
    // Instanz gelegt — Entitaeten mit demselben Model laufen unabhaengig.
    for (EntityID id : mEntities) {
        auto* mr = GetComponent<ModelRendererComponent>(id);
        if (!mr || !mr->model || !mr->model->IsAnimated()) continue;

        if (mr->animClip < 0) {
            const int start = mr->model->GetAutostartClip();
            if (start < 0) continue;
            StartEntityClip(id, start); // erste Instanz-Pose sofort
        }
        if (!mr->animPlaying) continue;
        const AnimClip* clip = mr->model->GetClip(mr->animClip);
        if (!clip) {
            mr->animPlaying = false;
            continue;
        }
        EnsureInstanceMeshes(*mr);
        int a = 0, b = 0;
        float t = 0.0f;
        if (!Model::AdvanceClipState(*clip, dt, mr->animTime, mr->animPlaying,
                                     a, b, t))
            continue;
        for (size_t i = 0; i < mr->instanceMeshes.size(); ++i)
            mr->model->MorphToMesh(i, a, b, t, mr->instanceMeshes[i]);
    }
}

bool Scene::StartEntityClip(EntityID id, int clipIndex, bool restart) {
    auto* mr = GetComponent<ModelRendererComponent>(id);
    if (!mr || !mr->model || !mr->model->IsAnimated()) return false;
    const AnimClip* clip = mr->model->GetClip(clipIndex);
    if (!clip || clip->frames.empty()) {
        RPG_LOG_WARN("Scene::StartEntityClip - unbekannter Clip " +
                     std::to_string(clipIndex) + " auf Entitaet " +
                     std::to_string(id));
        return false;
    }
    if (mr->animClip == clipIndex && mr->animPlaying && !restart) return true;
    EnsureInstanceMeshes(*mr);
    mr->animClip = clipIndex;
    mr->animPlaying = true;
    mr->animTime = 0.0f;
    // Sprung sofort sichtbar: erster Clip-Frame in die Instanzpuffer
    for (size_t i = 0; i < mr->instanceMeshes.size(); ++i)
        mr->model->MorphToMesh(i, clip->frames[0], clip->frames[0], 0.0f,
                               mr->instanceMeshes[i]);
    return true;
}

bool Scene::StartEntityClip(EntityID id, const std::string& clipName,
                            bool restart) {
    auto* mr = GetComponent<ModelRendererComponent>(id);
    if (!mr || !mr->model) return false;
    return StartEntityClip(id, mr->model->FindClip(clipName), restart);
}

bool Scene::StopEntityClip(EntityID id) {
    auto* mr = GetComponent<ModelRendererComponent>(id);
    if (!mr || mr->animClip < 0) return false;
    mr->animPlaying = false;
    return true;
}

void Scene::Clear() {
    mEntities.clear();
    mNames.clear();
    mComponents.clear();
    mNextID = 1;
}

} // namespace rpg
