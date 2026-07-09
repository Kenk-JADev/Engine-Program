#include "rpgmaker3d/ResourceManager.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Shader.h"

namespace rpg {

ResourceManager::ResourceManager() = default;

ResourceManager::~ResourceManager() {
    ClearAll();
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& path) {
    auto it = mTextures.find(path);
    if (it != mTextures.end()) {
        if (auto sp = it->second.lock()) return sp;
    }

    auto tex = std::make_shared<Texture>();
    if (!tex->LoadFromFile(path)) {
        tex->CreateCheckerboard();
    }
    mTextures[path] = tex;
    return tex;
}

std::shared_ptr<Model> ResourceManager::GetModel(const std::string& path) {
    auto it = mModels.find(path);
    if (it != mModels.end()) {
        if (auto sp = it->second.lock()) return sp;
    }

    auto model = std::make_shared<Model>();
    if (!model->LoadFromOBJ(path)) {
        // Fallback cube
        auto mesh = MeshFactory::CreateCube(1.0f);
        // Cube bleibt eigenständig – hier einfach ein leeres Model zurückgeben
    }
    mModels[path] = model;
    return model;
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& name) {
    auto it = mShaders.find(name);
    if (it != mShaders.end()) return it->second;
    return nullptr;
}

void ResourceManager::RegisterShader(const std::string& name, std::shared_ptr<Shader> shader) {
    mShaders[name] = shader;
}

void ResourceManager::ReloadTexture(const std::string& path) {
    mTextures.erase(path);
}

void ResourceManager::ClearUnused() {
    for (auto it = mTextures.begin(); it != mTextures.end(); ) {
        if (it->second.expired()) it = mTextures.erase(it);
        else ++it;
    }
    for (auto it = mModels.begin(); it != mModels.end(); ) {
        if (it->second.expired()) it = mModels.erase(it);
        else ++it;
    }
}

void ResourceManager::ClearAll() {
    mTextures.clear();
    mModels.clear();
    mShaders.clear();
}

} // namespace rpg
