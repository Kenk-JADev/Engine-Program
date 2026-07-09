#pragma once

#include <string>

struct mrb_state;

typedef unsigned int GLuint;

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

    mrb_state* GetState() { return mMrb; }

private:
    void BindEngine();
    void BindInput();
    void BindAudio();
    void BindMap();
    void BindActor();

    mrb_state* mMrb = nullptr;
    Engine* mEngine = nullptr;
};

} // namespace rpg
