#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"

#ifdef RPGMAKER3D_ENABLE_RUBY
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <iostream>

namespace rpg {

static void DeleteEngine(mrb_state* mrb) { (void)mrb; }

RubyVM::RubyVM() = default;

RubyVM::~RubyVM() {
    Shutdown();
}

bool RubyVM::Initialize(Engine* engine) {
    mEngine = engine;
    mMrb = mrb_open();
    if (!mMrb) {
        RPG_LOG_ERROR("Failed to open mruby VM");
        return false;
    }
    BindEngine();
    BindInput();
    BindAudio();
    BindMap();
    BindActor();
    BindCamera();
    BindGame();
    RPG_LOG_INFO("Ruby VM initialized");
    return true;
}

void RubyVM::Shutdown() {
    if (mMrb) {
        mrb_close(mMrb);
        mMrb = nullptr;
    }
}

bool RubyVM::ExecuteString(const std::string& code) {
    if (!mMrb) return false;
    mrb_load_string(mMrb, code.c_str());
    if (mMrb->exc) {
        mrb_print_error(mMrb);
        mMrb->exc = nullptr;
        return false;
    }
    return true;
}

bool RubyVM::ExecuteFile(const std::string& path) {
    if (!mMrb) return false;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        RPG_LOG_ERROR("Failed to open script: " + path);
        return false;
    }
    mrb_load_file(mMrb, f);
    fclose(f);
    if (mMrb->exc) {
        mrb_print_error(mMrb);
        mMrb->exc = nullptr;
        return false;
    }
    return true;
}

bool RubyVM::Update(float deltaTime) {
    if (!mMrb) return false;
    mrb_sym sym = mrb_intern_lit(mMrb, "$game");
    mrb_value game = mrb_gv_get(mMrb, sym);
    if (mrb_nil_p(game)) return true;

    mrb_sym updateSym = mrb_intern_lit(mMrb, "update");
    mrb_value dt = mrb_float_value(mMrb, deltaTime);
    mrb_funcall_argv(mMrb, game, updateSym, 1, &dt);
    if (mMrb->exc) {
        mrb_print_error(mMrb);
        mMrb->exc = nullptr;
        return false;
    }
    return true;
}

// ========== Input Bindings ==========
static mrb_value rb_input_key_down(mrb_state* mrb, mrb_value self) {
    mrb_sym keySym;
    mrb_get_args(mrb, "n", &keySym);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_bool_value(false);

    std::string keyName(mrb_sym2name(mrb, keySym));
    Key key = Key::Unknown;
    if (keyName == "w" || keyName == "W") key = Key::W;
    else if (keyName == "a" || keyName == "A") key = Key::A;
    else if (keyName == "s" || keyName == "S") key = Key::S;
    else if (keyName == "d" || keyName == "D") key = Key::D;
    else if (keyName == "space") key = Key::Space;
    else if (keyName == "return" || keyName == "enter") key = Key::Enter;
    else if (keyName == "escape") key = Key::Escape;

    return mrb_bool_value(engine->GetInput().IsKeyDown(key));
}

void RubyVM::BindInput() {
    struct RClass* input = mrb_define_module(mMrb, "Input");
    mrb_define_module_function(mMrb, input, "key_down?", rb_input_key_down, MRB_ARGS_REQ(1));
}

// ========== Audio Bindings ==========
static mrb_value rb_audio_play_music(mrb_state* mrb, mrb_value self) {
    char* path;
    mrb_bool loop = true;
    mrb_get_args(mrb, "z|b", &path, &loop);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetAudio().PlayMusic(path, loop);
    return mrb_nil_value();
}

static mrb_value rb_audio_play_sound(mrb_state* mrb, mrb_value self) {
    char* path;
    mrb_bool loop = false;
    mrb_get_args(mrb, "z|b", &path, &loop);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) {
        engine->GetAudio().LoadSound("ruby_sound", path);
        engine->GetAudio().PlaySound("ruby_sound", loop);
    }
    return mrb_nil_value();
}

static mrb_value rb_audio_stop_music(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetAudio().StopMusic();
    return mrb_nil_value();
}

void RubyVM::BindAudio() {
    struct RClass* audio = mrb_define_module(mMrb, "Audio");
    mrb_define_module_function(mMrb, audio, "bgm_play", rb_audio_play_music, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, audio, "se_play", rb_audio_play_sound, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, audio, "bgm_stop", rb_audio_stop_music, MRB_ARGS_NONE());
}

// ========== Map Bindings ==========
static mrb_value rb_map_set_tile(mrb_state* mrb, mrb_value self) {
    mrb_int layer, x, z, tile;
    mrb_get_args(mrb, "iiii", &layer, &x, &z, &tile);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetMap().SetTile(static_cast<int>(layer), static_cast<int>(x), static_cast<int>(z), static_cast<int>(tile));
    return mrb_nil_value();
}

static mrb_value rb_map_get_tile(mrb_state* mrb, mrb_value self) {
    mrb_int layer, x, z;
    mrb_get_args(mrb, "iii", &layer, &x, &z);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_int_value(mrb, -1);
    return mrb_int_value(mrb, engine->GetMap().GetTile(static_cast<int>(layer), static_cast<int>(x), static_cast<int>(z)));
}

void RubyVM::BindMap() {
    struct RClass* map = mrb_define_module(mMrb, "Map");
    mrb_define_module_function(mMrb, map, "set_tile", rb_map_set_tile, MRB_ARGS_REQ(4));
    mrb_define_module_function(mMrb, map, "get_tile", rb_map_get_tile, MRB_ARGS_REQ(3));
}

// ========== Actor Bindings ==========
static void actor_free(mrb_state* mrb, void* p) {
    (void)mrb;
    delete static_cast<EntityID*>(p);
}

static const mrb_data_type actor_type = { "Actor", actor_free };

static mrb_value rb_actor_new(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_nil_value();

    EntityID id = engine->GetScene().CreateEntity(name);
    auto* transform = engine->GetScene().AddComponent<TransformComponent>(id);
    transform->transform.position = Vec3(0, 0, 0);

    EntityID* ptr = new EntityID(id);
    return mrb_obj_value(mrb_data_object_alloc(mrb, mrb_class_ptr(self), ptr, &actor_type));
}

static mrb_value rb_actor_move_to(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) transform->transform.position = Vec3(x, y, z);
    }
    return self;
}

static mrb_value rb_actor_move(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) transform->transform.position += Vec3(x, y, z);
    }
    return self;
}

static mrb_value rb_actor_position(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) {
            mrb_value arr = mrb_ary_new(mrb);
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.position.x));
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.position.y));
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.position.z));
            return arr;
        }
    }
    return mrb_nil_value();
}

static mrb_value rb_actor_rotation(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) {
            mrb_value arr = mrb_ary_new(mrb);
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.rotation.x));
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.rotation.y));
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.rotation.z));
            return arr;
        }
    }
    return mrb_nil_value();
}

static mrb_value rb_actor_scale(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) {
            mrb_value arr = mrb_ary_new(mrb);
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.scale.x));
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.scale.y));
            mrb_ary_push(mrb, arr, mrb_float_value(mrb, transform->transform.scale.z));
            return arr;
        }
    }
    return mrb_nil_value();
}

static mrb_value rb_actor_set_rotation(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) transform->transform.rotation = Vec3(x, y, z);
    }
    return self;
}

static mrb_value rb_actor_set_scale(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* transform = engine->GetScene().GetComponent<TransformComponent>(*id);
        if (transform) transform->transform.scale = Vec3(x, y, z);
    }
    return self;
}

static mrb_value rb_actor_name(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        return mrb_str_new_cstr(mrb, engine->GetScene().GetEntityName(*id).c_str());
    }
    return mrb_nil_value();
}

static mrb_value rb_actor_set_model(mrb_state* mrb, mrb_value self) {
    char* type;
    mrb_get_args(mrb, "z", &type);
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* model = engine->GetScene().GetComponent<ModelRendererComponent>(*id);
        if (!model) {
            model = engine->GetScene().AddComponent<ModelRendererComponent>(*id);
        }
        model->model = std::make_shared<Model>();
        std::string t(type);
        if (t == "plane") model->model->AddMesh(MeshFactory::CreatePlane(2.0f));
        else model->model->AddMesh(MeshFactory::CreateCube(1.0f));
    }
    return self;
}

static mrb_value rb_actor_set_color(mrb_state* mrb, mrb_value self) {
    mrb_float r, g, b;
    mrb_get_args(mrb, "fff", &r, &g, &b);
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (id && engine) {
        auto* model = engine->GetScene().GetComponent<ModelRendererComponent>(*id);
        if (model) {
            model->texture.reset();
            auto* material = engine->GetScene().GetComponent<MaterialComponent>(*id);
            if (!material) material = engine->GetScene().AddComponent<MaterialComponent>(*id);
            material->material.diffuse = Color(r, g, b, 1.0f);
        }
    }
    return self;
}

void RubyVM::BindActor() {
    struct RClass* actor = mrb_define_class(mMrb, "Actor", mMrb->object_class);
    mrb_define_class_method(mMrb, actor, "new", rb_actor_new, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, actor, "move_to", rb_actor_move_to, MRB_ARGS_REQ(3));
    mrb_define_method(mMrb, actor, "move", rb_actor_move, MRB_ARGS_REQ(3));
    mrb_define_method(mMrb, actor, "position", rb_actor_position, MRB_ARGS_NONE());
    mrb_define_method(mMrb, actor, "rotation", rb_actor_rotation, MRB_ARGS_NONE());
    mrb_define_method(mMrb, actor, "scale", rb_actor_scale, MRB_ARGS_NONE());
    mrb_define_method(mMrb, actor, "set_rotation", rb_actor_set_rotation, MRB_ARGS_REQ(3));
    mrb_define_method(mMrb, actor, "set_scale", rb_actor_set_scale, MRB_ARGS_REQ(3));
    mrb_define_method(mMrb, actor, "name", rb_actor_name, MRB_ARGS_NONE());
    mrb_define_method(mMrb, actor, "set_model", rb_actor_set_model, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, actor, "set_color", rb_actor_set_color, MRB_ARGS_REQ(3));
}

static mrb_value rb_engine_time(mrb_state* mrb, mrb_value self) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_float_value(mrb, 0.0f);
    return mrb_float_value(mrb, engine->GetTime());
}

static mrb_value rb_engine_delta_time(mrb_state* mrb, mrb_value self) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_float_value(mrb, 0.0f);
    return mrb_float_value(mrb, engine->GetDeltaTime());
}

static mrb_value rb_engine_log(mrb_state* mrb, mrb_value self) {
    char* msg;
    mrb_get_args(mrb, "z", &msg);
    RPG_LOG_INFO(std::string(msg));
    return mrb_nil_value();
}

static mrb_value rb_camera_position(mrb_state* mrb, mrb_value self) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_nil_value();
    auto pos = engine->GetRenderer().GetCamera().GetPosition();
    mrb_value arr = mrb_ary_new(mrb);
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, pos.x));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, pos.y));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, pos.z));
    return arr;
}

static mrb_value rb_camera_set_position(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetRenderer().GetCamera().SetPosition(Vec3(x, y, z));
    return mrb_nil_value();
}

static mrb_value rb_camera_rotation(mrb_state* mrb, mrb_value self) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_nil_value();
    auto rot = engine->GetRenderer().GetCamera().GetRotation();
    mrb_value arr = mrb_ary_new(mrb);
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, rot.x));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, rot.y));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, rot.z));
    return arr;
}

static mrb_value rb_camera_set_rotation(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetRenderer().GetCamera().SetRotation(Vec3(x, y, z));
    return mrb_nil_value();
}

static mrb_value rb_map_width(mrb_state* mrb, mrb_value self) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_int_value(mrb, 0);
    return mrb_int_value(mrb, engine->GetMap().GetWidth());
}

static mrb_value rb_map_height(mrb_state* mrb, mrb_value self) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return mrb_int_value(mrb, 0);
    return mrb_int_value(mrb, engine->GetMap().GetHeight());
}

void RubyVM::BindEngine() {
    mMrb->ud = mEngine;
}

void RubyVM::BindGame() {
    struct RClass* engine = mrb_define_module(mMrb, "Engine");
    mrb_define_module_function(mMrb, engine, "time", rb_engine_time, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, engine, "delta_time", rb_engine_delta_time, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, engine, "log", rb_engine_log, MRB_ARGS_REQ(1));
}

void RubyVM::BindCamera() {
    struct RClass* camera = mrb_define_module(mMrb, "Camera");
    mrb_define_module_function(mMrb, camera, "position", rb_camera_position, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, camera, "position=", rb_camera_set_position, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, camera, "rotation", rb_camera_rotation, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, camera, "rotation=", rb_camera_set_rotation, MRB_ARGS_REQ(1));
}

void RubyVM::BindMap() {
    struct RClass* map = mrb_define_module(mMrb, "Map");
    mrb_define_module_function(mMrb, map, "set_tile", rb_map_set_tile, MRB_ARGS_REQ(4));
    mrb_define_module_function(mMrb, map, "get_tile", rb_map_get_tile, MRB_ARGS_REQ(3));
    mrb_define_module_function(mMrb, map, "width", rb_map_width, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, map, "height", rb_map_height, MRB_ARGS_NONE());
}

} // namespace rpg

#else // !RPGMAKER3D_ENABLE_RUBY

namespace rpg {

RubyVM::RubyVM() = default;
RubyVM::~RubyVM() = default;

bool RubyVM::Initialize(Engine* engine) {
    (void)engine;
    RPG_LOG_WARN("Ruby support not compiled in. Set RPGMAKER3D_ENABLE_RUBY=ON");
    return false;
}

void RubyVM::Shutdown() {}
bool RubyVM::ExecuteString(const std::string& code) { (void)code; return false; }
bool RubyVM::ExecuteFile(const std::string& path) { (void)path; return false; }
bool RubyVM::Update(float deltaTime) { (void)deltaTime; return false; }
void RubyVM::BindEngine() {}
void RubyVM::BindInput() {}
void RubyVM::BindAudio() {}
void RubyVM::BindMap() {}
void RubyVM::BindActor() {}
void RubyVM::BindCamera() {}
void RubyVM::BindGame() {}

} // namespace rpg

#endif
