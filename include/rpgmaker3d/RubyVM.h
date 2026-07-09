#pragma once

#include <string>

struct mrb_state;

namespace rpg {

class Engine;

class RubyVM {
public:
    RubyVM();
    ~RubyVM();

    bool Initialize(Engine* engine);
    void Shutdown();

    bool ExecuteString(const std::string& code);
    bool ExecuteFile(const std::string& path);

    void BindEngine();
    void RegisterClass(const std::string& name);

    mrb_state* GetState() { return mMrb; }

private:
    mrb_state* mMrb = nullptr;
    Engine* mEngine = nullptr;
};

} // namespace rpg
