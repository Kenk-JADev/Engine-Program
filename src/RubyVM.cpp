#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Model.h"

#ifdef RPGMAKER3D_ENABLE_RUBY

#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/hash.h>

#include <cstdio>
#include <iostream>
#include <memory>

namespace rpg {

// ==================== Core ====================

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

    mMrb->ud = mEngine;

    BindEngine();
    BindGame();
    BindInput();
    BindAudio();
    BindMap();
    BindActor();
    BindCamera();

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
    if (!mMrb) {
        return false;
    }

    mrb_load_string(mMrb, code.c_str());

    if (mMrb->exc) {
        mrb_print_error(mMrb);
        mMrb->exc = nullptr;
        return false;
    }

    return true;
}

bool RubyVM::ExecuteFile(const std::string& path) {
    if (!mMrb) {
        return false;
    }

    FILE* file = fopen(path.c_str(), "r");

    if (!file) {
        RPG_LOG_ERROR("Failed to open script: " + path);
        return false;
    }

    mrb_load_file(mMrb, file);
    fclose(file);

    if (mMrb->exc) {
        mrb_print_error(mMrb);
        mMrb->exc = nullptr;
        return false;
    }

    return true;
}

bool RubyVM::Update(float deltaTime) {
    if (!mMrb) {
        return false;
    }

    mrb_sym gameSymbol = mrb_intern_lit(mMrb, "$game");
    mrb_value game = mrb_gv_get(mMrb, gameSymbol);

    if (mrb_nil_p(game)) {
        return true;
    }

    mrb_sym updateSymbol = mrb_intern_lit(mMrb, "update");
    mrb_value deltaValue = mrb_float_value(mMrb, deltaTime);

    mrb_funcall_argv(mMrb, game, updateSymbol, 1, &deltaValue);

    if (mMrb->exc) {
        mrb_print_error(mMrb);
        mMrb->exc = nullptr;
        return false;
    }

    return true;
}

// ==================== Engine / Game Bindings ====================

static mrb_value rb_engine_time(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_float_value(mrb, 0.0f);
    }

    return mrb_float_value(mrb, engine->GetTime());
}

static mrb_value rb_engine_delta_time(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_float_value(mrb, 0.0f);
    }

    return mrb_float_value(mrb, engine->GetDeltaTime());
}

static mrb_value rb_engine_log(mrb_state* mrb, mrb_value self) {
    (void)self;

    char* message = nullptr;
    mrb_get_args(mrb, "z", &message);

    if (message) {
        RPG_LOG_INFO(std::string(message));
    }

    return mrb_nil_value();
}

void RubyVM::BindEngine() {
    // mMrb->ud wird bereits in Initialize gesetzt.
}

void RubyVM::BindGame() {
    struct RClass* engineModule = mrb_define_module(mMrb, "Engine");

    mrb_define_module_function(
        mMrb,
        engineModule,
        "time",
        rb_engine_time,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        engineModule,
        "delta_time",
        rb_engine_delta_time,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        engineModule,
        "log",
        rb_engine_log,
        MRB_ARGS_REQ(1)
    );
}

// ==================== Input Bindings ====================

static mrb_value rb_input_key_down(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_sym keySymbol;
    mrb_get_args(mrb, "n", &keySymbol);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_bool_value(false);
    }

    const char* rawKeyName = mrb_sym2name(mrb, keySymbol);
    std::string keyName = rawKeyName ? rawKeyName : "";

    Key key = Key::Unknown;

    if (keyName == "w" || keyName == "W") {
        key = Key::W;
    } else if (keyName == "a" || keyName == "A") {
        key = Key::A;
    } else if (keyName == "s" || keyName == "S") {
        key = Key::S;
    } else if (keyName == "d" || keyName == "D") {
        key = Key::D;
    } else if (keyName == "space") {
        key = Key::Space;
    } else if (keyName == "return" || keyName == "enter") {
        key = Key::Enter;
    } else if (keyName == "escape") {
        key = Key::Escape;
    } else if (keyName == "e" || keyName == "E") {
        key = Key::E;
    } else if (keyName == "q" || keyName == "Q") {
        key = Key::Q;
    }

    return mrb_bool_value(engine->GetInput().IsKeyDown(key));
}

void RubyVM::BindInput() {
    struct RClass* inputModule = mrb_define_module(mMrb, "Input");

    mrb_define_module_function(
        mMrb,
        inputModule,
        "key_down?",
        rb_input_key_down,
        MRB_ARGS_REQ(1)
    );

    mrb_define_module_function(
        mMrb,
        inputModule,
        "key_down",
        rb_input_key_down,
        MRB_ARGS_REQ(1)
    );
}

// ==================== Audio Bindings ====================

static mrb_value rb_audio_play_music(mrb_state* mrb, mrb_value self) {
    (void)self;

    char* path = nullptr;
    mrb_bool loop = true;

    mrb_get_args(mrb, "z|b", &path, &loop);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine && path) {
        engine->GetAudio().PlayMusic(path, loop);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_play_sound(mrb_state* mrb, mrb_value self) {
    (void)self;

    char* path = nullptr;
    mrb_bool loop = false;

    mrb_get_args(mrb, "z|b", &path, &loop);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine && path) {
        engine->GetAudio().LoadSound("ruby_sound", path);
        engine->GetAudio().PlaySound("ruby_sound", loop);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_stop_music(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().StopMusic();
    }

    return mrb_nil_value();
}

void RubyVM::BindAudio() {
    struct RClass* audioModule = mrb_define_module(mMrb, "Audio");

    mrb_define_module_function(
        mMrb,
        audioModule,
        "bgm_play",
        rb_audio_play_music,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "se_play",
        rb_audio_play_sound,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "bgm_stop",
        rb_audio_stop_music,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "play_music",
        rb_audio_play_music,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "play_sound",
        rb_audio_play_sound,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(1)
    );
}

// ==================== Map Bindings ====================

static mrb_value rb_map_set_tile(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_int layer;
    mrb_int x;
    mrb_int z;
    mrb_int tile;

    mrb_get_args(mrb, "iiii", &layer, &x, &z, &tile);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetMap().SetTile(
            static_cast<int>(layer),
            static_cast<int>(x),
            static_cast<int>(z),
            static_cast<int>(tile)
        );
    }

    return mrb_nil_value();
}

static mrb_value rb_map_get_tile(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_int layer;
    mrb_int x;
    mrb_int z;

    mrb_get_args(mrb, "iii", &layer, &x, &z);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_int_value(mrb, -1);
    }

    return mrb_int_value(
        mrb,
        engine->GetMap().GetTile(
            static_cast<int>(layer),
            static_cast<int>(x),
            static_cast<int>(z)
        )
    );
}

static mrb_value rb_map_width(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_int_value(mrb, 0);
    }

    return mrb_int_value(mrb, engine->GetMap().GetWidth());
}

static mrb_value rb_map_height(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_int_value(mrb, 0);
    }

    return mrb_int_value(mrb, engine->GetMap().GetHeight());
}

void RubyVM::BindMap() {
    struct RClass* mapModule = mrb_define_module(mMrb, "Map");

    mrb_define_module_function(
        mMrb,
        mapModule,
        "set_tile",
        rb_map_set_tile,
        MRB_ARGS_REQ(4)
    );

    mrb_define_module_function(
        mMrb,
        mapModule,
        "get_tile",
        rb_map_get_tile,
        MRB_ARGS_REQ(3)
    );

    mrb_define_module_function(
        mMrb,
        mapModule,
        "width",
        rb_map_width,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        mapModule,
        "height",
        rb_map_height,
        MRB_ARGS_NONE()
    );
}

// ==================== Camera Bindings ====================

static mrb_value rb_camera_position(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_nil_value();
    }

    const auto position = engine->GetRenderer().GetCamera().GetPosition();

    mrb_value result = mrb_ary_new(mrb);

    mrb_ary_push(mrb, result, mrb_float_value(mrb, position.x));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, position.y));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, position.z));

    return result;
}

static mrb_value rb_camera_set_position(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_float x;
    mrb_float y;
    mrb_float z;

    mrb_get_args(mrb, "fff", &x, &y, &z);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetRenderer().GetCamera().SetPosition(Vec3(x, y, z));
    }

    return mrb_nil_value();
}

static mrb_value rb_camera_rotation(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_nil_value();
    }

    const auto rotation = engine->GetRenderer().GetCamera().GetRotation();

    mrb_value result = mrb_ary_new(mrb);

    mrb_ary_push(mrb, result, mrb_float_value(mrb, rotation.x));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, rotation.y));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, rotation.z));

    return result;
}

static mrb_value rb_camera_set_rotation(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_float x;
    mrb_float y;
    mrb_float z;

    mrb_get_args(mrb, "fff", &x, &y, &z);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetRenderer().GetCamera().SetRotation(Vec3(x, y, z));
    }

    return mrb_nil_value();
}

void RubyVM::BindCamera() {
    struct RClass* cameraModule = mrb_define_module(mMrb, "Camera");

    mrb_define_module_function(
        mMrb,
        cameraModule,
        "position",
        rb_camera_position,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        cameraModule,
        "set_position",
        rb_camera_set_position,
        MRB_ARGS_REQ(3)
    );

    mrb_define_module_function(
        mMrb,
        cameraModule,
        "rotation",
        rb_camera_rotation,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        cameraModule,
        "set_rotation",
        rb_camera_set_rotation,
        MRB_ARGS_REQ(3)
    );
}

// ==================== Actor Bindings ====================

static void actor_free(mrb_state* mrb, void* pointer) {
    (void)mrb;
    delete static_cast<EntityID*>(pointer);
}

static const mrb_data_type actor_type = {
    "Actor",
    actor_free
};

static mrb_value rb_actor_new(mrb_state* mrb, mrb_value self) {
    char* name = nullptr;
    mrb_get_args(mrb, "z", &name);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine || !name) {
        return mrb_nil_value();
    }

    EntityID id = engine->GetScene().CreateEntity(name);

    auto* transform = engine->GetScene().AddComponent<TransformComponent>(id);
    transform->transform.position = Vec3(0.0f, 0.0f, 0.0f);

    EntityID* data = new EntityID(id);

    return mrb_obj_value(
        mrb_data_object_alloc(
            mrb,
            mrb_class_ptr(self),
            data,
            &actor_type
        )
    );
}

static mrb_value rb_actor_move_to(mrb_state* mrb, mrb_value self) {
    mrb_float x;
    mrb_float y;
    mrb_float z;

    mrb_get_args(mrb, "fff", &x, &y, &z);

    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            transform->transform.position = Vec3(x, y, z);
        }
    }

    return self;
}

static mrb_value rb_actor_move(mrb_state* mrb, mrb_value self) {
    mrb_float x;
    mrb_float y;
    mrb_float z;

    mrb_get_args(mrb, "fff", &x, &y, &z);

    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            transform->transform.position += Vec3(x, y, z);
        }
    }

    return self;
}

static mrb_value rb_actor_position(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            mrb_value result = mrb_ary_new(mrb);

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.position.x)
            );

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.position.y)
            );

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.position.z)
            );

            return result;
        }
    }

    return mrb_nil_value();
}

static mrb_value rb_actor_rotation(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            mrb_value result = mrb_ary_new(mrb);

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.rotation.x)
            );

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.rotation.y)
            );

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.rotation.z)
            );

            return result;
        }
    }

    return mrb_nil_value();
}

static mrb_value rb_actor_scale(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            mrb_value result = mrb_ary_new(mrb);

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.scale.x)
            );

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.scale.y)
            );

            mrb_ary_push(
                mrb,
                result,
                mrb_float_value(mrb, transform->transform.scale.z)
            );

            return result;
        }
    }

    return mrb_nil_value();
}

static mrb_value rb_actor_set_rotation(mrb_state* mrb, mrb_value self) {
    mrb_float x;
    mrb_float y;
    mrb_float z;

    mrb_get_args(mrb, "fff", &x, &y, &z);

    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            transform->transform.rotation = Vec3(x, y, z);
        }
    }

    return self;
}

static mrb_value rb_actor_set_scale(mrb_state* mrb, mrb_value self) {
    mrb_float x;
    mrb_float y;
    mrb_float z;

    mrb_get_args(mrb, "fff", &x, &y, &z);

    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* transform =
            engine->GetScene().GetComponent<TransformComponent>(*id);

        if (transform) {
            transform->transform.scale = Vec3(x, y, z);
        }
    }

    return self;
}

static mrb_value rb_actor_name(mrb_state* mrb, mrb_value self) {
    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        return mrb_str_new_cstr(
            mrb,
            engine->GetScene().GetEntityName(*id).c_str()
        );
    }

    return mrb_nil_value();
}

static mrb_value rb_actor_set_model(mrb_state* mrb, mrb_value self) {
    char* type = nullptr;
    mrb_get_args(mrb, "z", &type);

    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine && type) {
        auto* model =
            engine->GetScene().GetComponent<ModelRendererComponent>(*id);

        if (!model) {
            model =
                engine->GetScene().AddComponent<ModelRendererComponent>(*id);
        }

        model->model = std::make_shared<Model>();

        const std::string modelType(type);

        if (modelType == "plane") {
            model->model->AddMesh(MeshFactory::CreatePlane(2.0f));
        } else {
            model->model->AddMesh(MeshFactory::CreateCube(1.0f));
        }
    }

    return self;
}

static mrb_value rb_actor_set_color(mrb_state* mrb, mrb_value self) {
    mrb_float red;
    mrb_float green;
    mrb_float blue;

    mrb_get_args(mrb, "fff", &red, &green, &blue);

    EntityID* id = static_cast<EntityID*>(DATA_PTR(self));
    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (id && engine) {
        auto* material =
            engine->GetScene().GetComponent<MaterialComponent>(*id);

        if (!material) {
            material =
                engine->GetScene().AddComponent<MaterialComponent>(*id);
        }

        material->material.diffuse = Color(red, green, blue, 1.0f);
    }

    return self;
}

void RubyVM::BindActor() {
    struct RClass* actorClass =
        mrb_define_class(mMrb, "Actor", mMrb->object_class);

    MRB_SET_INSTANCE_TT(actorClass, MRB_TT_DATA);

    mrb_define_class_method(
        mMrb,
        actorClass,
        "new",
        rb_actor_new,
        MRB_ARGS_REQ(1)
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "move_to",
        rb_actor_move_to,
        MRB_ARGS_REQ(3)
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "move",
        rb_actor_move,
        MRB_ARGS_REQ(3)
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "position",
        rb_actor_position,
        MRB_ARGS_NONE()
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "rotation",
        rb_actor_rotation,
        MRB_ARGS_NONE()
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "scale",
        rb_actor_scale,
        MRB_ARGS_NONE()
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "set_rotation",
        rb_actor_set_rotation,
        MRB_ARGS_REQ(3)
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "set_scale",
        rb_actor_set_scale,
        MRB_ARGS_REQ(3)
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "name",
        rb_actor_name,
        MRB_ARGS_NONE()
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "set_model",
        rb_actor_set_model,
        MRB_ARGS_REQ(1)
    );

    mrb_define_method(
        mMrb,
        actorClass,
        "set_color",
        rb_actor_set_color,
        MRB_ARGS_REQ(3)
    );
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

bool RubyVM::ExecuteString(const std::string& code) {
    (void)code;
    return false;
}

bool RubyVM::ExecuteFile(const std::string& path) {
    (void)path;
    return false;
}

bool RubyVM::Update(float deltaTime) {
    (void)deltaTime;
    return false;
}

void RubyVM::BindEngine() {}
void RubyVM::BindInput() {}
void RubyVM::BindAudio() {}
void RubyVM::BindMap() {}
void RubyVM::BindActor() {}
void RubyVM::BindCamera() {}
void RubyVM::BindGame() {}

} // namespace rpg

#endif
