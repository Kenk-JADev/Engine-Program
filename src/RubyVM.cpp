#include "rpgmaker3d/RubyVM.h"

#ifdef RPGMAKER3D_ENABLE_RUBY
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <iostream>
#else
#include <iostream>
#endif

namespace rpg {

RubyVM::RubyVM() = default;

RubyVM::~RubyVM() {
    Shutdown();
}

bool RubyVM::Initialize(Engine* engine) {
    mEngine = engine;
#ifdef RPGMAKER3D_ENABLE_RUBY
    mMrb = mrb_open();
    if (!mMrb) {
        std::cerr << "Failed to open mruby VM" << std::endl;
        return false;
    }
    BindEngine();
    return true;
#else
    std::cerr << "Ruby support not compiled in" << std::endl;
    return false;
#endif
}

void RubyVM::Shutdown() {
#ifdef RPGMAKER3D_ENABLE_RUBY
    if (mMrb) {
        mrb_close(mMrb);
        mMrb = nullptr;
    }
#endif
}

bool RubyVM::ExecuteString(const std::string& code) {
#ifdef RPGMAKER3D_ENABLE_RUBY
    if (!mMrb) return false;
    mrb_load_string(mMrb, code.c_str());
    if (mMrb->exc) {
        mrb_print_error(mMrb);
        return false;
    }
    return true;
#else
    (void)code;
    return false;
#endif
}

bool RubyVM::ExecuteFile(const std::string& path) {
#ifdef RPGMAKER3D_ENABLE_RUBY
    if (!mMrb) return false;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    mrb_load_file(mMrb, f);
    fclose(f);
    return mMrb->exc == nullptr;
#else
    (void)path;
    return false;
#endif
}

void RubyVM::BindEngine() {
#ifdef RPGMAKER3D_ENABLE_RUBY
    // Hier würden C++-Bindings für Actor, Map, Audio etc. definiert werden.
#endif
}

void RubyVM::RegisterClass(const std::string& name) {
#ifdef RPGMAKER3D_ENABLE_RUBY
    if (!mMrb) return;
    struct RClass* c = mrb_define_class(mMrb, name.c_str(), mMrb->object_class);
    (void)c;
#else
    (void)name;
#endif
}

} // namespace rpg
