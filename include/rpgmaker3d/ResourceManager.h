#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "Types.h"

namespace rpg {

class Texture;
class Model;
class Shader;

class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    std::shared_ptr<Texture> GetTexture(const std::string& path);
    std::shared_ptr<Model> GetModel(const std::string& path);
    std::shared_ptr<Shader> GetShader(const std::string& name);

    void RegisterShader(const std::string& name, std::shared_ptr<Shader> shader);
    void ReloadTexture(const std::string& path);
    void ClearUnused();
    void ClearAll();

private:
    std::unordered_map<std::string, std::weak_ptr<Texture>> mTextures;
    std::unordered_map<std::string, std::weak_ptr<Model>> mModels;
    std::unordered_map<std::string, std::shared_ptr<Shader>> mShaders;
};

} // namespace rpg
