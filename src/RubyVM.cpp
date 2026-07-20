#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/RmlUiSystem.h"

// Fix ssize_t for MSVC mruby build - must be before mruby headers.
// mruby expects the POSIX type ssize_t, which MSVC/Windows SDK does not
// provide anywhere. Define it unconditionally from std::intptr_t (same width
// as Windows SSIZE_T on both x86 and x64).
// NOTE: Do NOT try to detect SSIZE_T via _BASETSD_H_ here. If <windows.h> is
// pulled in inside a namespace (include-order bug), _BASETSD_H_ may be defined
// while ::SSIZE_T is not - exactly that combination broke the MSVC build.
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#ifndef _SSIZE_T_DEFINED
typedef std::intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif
// DO NOT define mrb_int_p / mrb_integer_p here to avoid macro redefinition warnings
// They will be defined after mruby includes as fallback
#else
// Linux/Mac: ssize_t already available via <unistd.h> or <sys/types.h> typically, but ensure
#ifndef _SSIZE_T_DEFINED
#include <unistd.h>
#endif
#endif

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

// mruby 4.0.0 compatibility fallbacks – define AFTER includes to avoid redefinition warnings
#ifndef mrb_int_p
  #ifdef mrb_integer_p
    #define mrb_int_p(o) mrb_integer_p(o)
  #else
    #define mrb_int_p(o) (mrb_type(o) == MRB_TT_INTEGER)
  #endif
#endif
#ifndef mrb_integer_p
  #define mrb_integer_p(o) (mrb_type(o) == MRB_TT_INTEGER)
#endif

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
    BindUI();

    RPG_LOG_INFO("Ruby VM initialized");
    return true;
}

void RubyVM::Shutdown() {
    if (mMrb) {
        mrb_close(mMrb);
        mMrb = nullptr;
    }
}

bool RubyVM::CaptureException(const std::string& context) {
    if (!mMrb || !mMrb->exc) {
        mLastError.clear();
        return false;
    }
    mrb_value exc = mrb_obj_value(mMrb->exc);
    mrb_value msg = mrb_funcall(mMrb, exc, "inspect", 0);
    std::string text;
    if (mrb_string_p(msg)) {
        text = std::string(RSTRING_PTR(msg), RSTRING_LEN(msg));
    } else {
        text = "(unprintable exception)";
    }
    mLastError = context.empty() ? text : (context + ": " + text);

    // Backtrace anhaengen (mruby-error Gem; bei Laueftzeitfehlern zeigt er
    // Datei/Zeile der Aufrufkette — genau das, was man zum Debuggen braucht)
    if (mMrb->exc) { // inspect koennte theoretisch selbst werfen
        mrb_value bt = mrb_funcall(mMrb, exc, "backtrace", 0);
        if (!mMrb->exc && mrb_array_p(bt)) {
            const mrb_int n = RARRAY_LEN(bt);
            std::string lines;
            for (mrb_int i = 0; i < n && i < 16; ++i) {
                mrb_value line = mrb_funcall(mMrb, mrb_ary_ref(mMrb, bt, i), "to_s", 0);
                if (mMrb->exc) { mMrb->exc = nullptr; break; }
                if (mrb_string_p(line)) {
                    lines += "\n  from ";
                    lines.append(RSTRING_PTR(line), RSTRING_LEN(line));
                }
            }
            if (!lines.empty())
                mLastError += "; Backtrace:" + lines;
        }
        if (mMrb->exc) mMrb->exc = nullptr; // Backtrace-Fehler ignorieren
    }

    RPG_LOG_ERROR("[Ruby] " + mLastError);
    mrb_print_error(mMrb);
    mMrb->exc = nullptr;
    return true;
}

bool RubyVM::ExecuteString(const std::string& code, const std::string& sourceName) {
    mLastError.clear();
    if (!mMrb) {
        mLastError = "RubyVM not initialized";
        return false;
    }

    // mrb_load_nstring_cxt mit filename fuer bessere Tracebacks
    mrbc_context* cxt = mrbc_context_new(mMrb);
    if (cxt && !sourceName.empty()) {
        mrbc_filename(mMrb, cxt, sourceName.c_str());
    }
    mrb_load_nstring_cxt(mMrb, code.c_str(), code.size(), cxt);
    if (cxt) mrbc_context_free(mMrb, cxt);

    if (mMrb->exc) {
        CaptureException(sourceName.empty() ? "<string>" : sourceName);
        return false;
    }
    return true;
}

bool RubyVM::ExecuteFile(const std::string& path) {
    mLastError.clear();
    if (!mMrb) {
        mLastError = "RubyVM not initialized";
        return false;
    }

    FILE* file = fopen(path.c_str(), "r");
    if (!file) {
        mLastError = "Failed to open script: " + path;
        RPG_LOG_ERROR(mLastError);
        return false;
    }

    mrbc_context* cxt = mrbc_context_new(mMrb);
    if (cxt) mrbc_filename(mMrb, cxt, path.c_str());
    mrb_load_file_cxt(mMrb, file, cxt);
    if (cxt) mrbc_context_free(mMrb, cxt);
    fclose(file);

    if (mMrb->exc) {
        CaptureException(path);
        return false;
    }
    return true;
}

bool RubyVM::CheckSyntax(const std::string& code, const std::string& sourceName,
                         std::string& errorOut) {
    errorOut.clear();
    if (!mMrb) {
        errorOut = "RubyVM not initialized";
        return false;
    }
    // Nur der Parser laeuft – der Code wird NICHT ausgefuehrt und erzeugt
    // auch keinen Bytecode. Syntaxfehler meldet mruby ueber den Parser
    // (mrb_parser_state, API von mruby 4.0.0, die die CI pinnt).
    mrbc_context* cxt = mrbc_context_new(mMrb);
    if (cxt && !sourceName.empty())
        mrbc_filename(mMrb, cxt, sourceName.c_str());
    mrb_parser_state* parser = mrb_parse_nstring(mMrb, code.c_str(), code.size(), cxt);
    bool ok = true;
    if (!parser || parser->nerr > 0) {
        ok = false;
        int line = 0;
        const char* msg = "(Syntaxfehler)";
        if (parser && parser->nerr > 0) {
            line = parser->error_buffer[0].lineno;
            if (parser->error_buffer[0].message)
                msg = parser->error_buffer[0].message;
        }
        errorOut = sourceName;
        if (line > 0)
            errorOut += ":" + std::to_string(line);
        errorOut += ": " + std::string(msg);
    }
    if (parser) mrb_parser_free(parser);
    if (cxt) mrbc_context_free(mMrb, cxt);
    if (mMrb->exc) mMrb->exc = nullptr; // Parser kann Exception hinterlassen
    return ok;
}

bool RubyVM::Update(float deltaTime) {
    if (!mMrb) {
        return false;
    }

    // 1) SceneManager (RPG-Maker-Style Scenes aus dem Script-Editor)
    //    Scene_Title / Scene_Map / Scene_Battle laufen hier pro Frame.
    {
        mrb_sym smSym = mrb_intern_lit(mMrb, "SceneManager");
        if (mrb_const_defined(mMrb, mrb_obj_value(mMrb->object_class), smSym)) {
            mrb_value sceneMgr = mrb_const_get(mMrb, mrb_obj_value(mMrb->object_class), smSym);
            if (!mrb_nil_p(sceneMgr)) {
                mrb_funcall(mMrb, sceneMgr, "update", 0);
                if (mMrb->exc) {
                    CaptureException("SceneManager.update");
                    return false;
                }
            }
        }
    }

    // 2) $game.update(dt) – optionale Custom-Logik aus main.rb
    mrb_sym gameSymbol = mrb_intern_lit(mMrb, "$game");
    mrb_value game = mrb_gv_get(mMrb, gameSymbol);
    if (!mrb_nil_p(game)) {
        mrb_sym updateSymbol = mrb_intern_lit(mMrb, "update");
        mrb_value deltaValue = mrb_float_value(mMrb, deltaTime);
        mrb_funcall_argv(mMrb, game, updateSymbol, 1, &deltaValue);
        if (mMrb->exc) {
            CaptureException("$game.update");
            return false;
        }
    }
    return true;
}

void RubyVM::CollectGarbage() {
    if (mMrb) {
        mrb_full_gc(mMrb);
    }
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
    } else if (keyName == "h" || keyName == "H") {
        key = Key::H;
    } else if (keyName == "f1") {
        key = Key::F1;
    } else if (keyName == "f2") {
        key = Key::F2;
    } else if (keyName == "f3") {
        key = Key::F3;
    } else if (keyName == "f5") {
        key = Key::F5;
    } else if (keyName == "up") {
        key = Key::Up;
    } else if (keyName == "down") {
        key = Key::Down;
    } else if (keyName == "left") {
        key = Key::Left;
    } else if (keyName == "right") {
        key = Key::Right;
    } else if (keyName == "i" || keyName == "I") {
        key = Key::I;
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
    mrb_float volume = 1.0f;
    mrb_float pitch = 1.0f;
    mrb_float fadeIn = 0.0f;

    mrb_get_args(mrb, "z|bfff", &path, &loop, &volume, &pitch, &fadeIn);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine && path) {
        engine->GetAudio().PlayBGM(path, loop, volume, pitch, fadeIn);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_play_sound(mrb_state* mrb, mrb_value self) {
    (void)self;

    char* path = nullptr;
    mrb_bool loop = false;
    mrb_float volume = 1.0f;
    mrb_float pitch = 1.0f;

    mrb_get_args(mrb, "z|bff", &path, &loop, &volume, &pitch);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine && path) {
        engine->GetAudio().PlaySE(path, loop, volume, pitch);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_play_bgs(mrb_state* mrb, mrb_value self) {
    (void)self;

    char* path = nullptr;
    mrb_bool loop = true;
    mrb_float volume = 1.0f;
    mrb_float pitch = 1.0f;
    mrb_float fadeIn = 0.0f;

    mrb_get_args(mrb, "z|bfff", &path, &loop, &volume, &pitch, &fadeIn);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine && path) {
        engine->GetAudio().PlayBGS(path, loop, volume, pitch, fadeIn);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_play_me(mrb_state* mrb, mrb_value self) {
    (void)self;

    char* path = nullptr;
    mrb_bool loop = false;
    mrb_float volume = 1.0f;
    mrb_float pitch = 1.0f;

    mrb_get_args(mrb, "z|bff", &path, &loop, &volume, &pitch);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine && path) {
        engine->GetAudio().PlayME(path, loop, volume, pitch);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_stop_music(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().FadeOutBGM(0.5f);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_stop_bgs(mrb_state* mrb, mrb_value self) {
    (void)self;

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().FadeOutBGS(0.5f);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_fade_out(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_float duration = 0.5f;
    mrb_get_args(mrb, "|f", &duration);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().FadeOutAll(duration);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_set_volume(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_float volume;
    mrb_get_args(mrb, "f", &volume);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().SetMasterVolume(volume);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_set_bgm_volume(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_float volume;
    mrb_get_args(mrb, "f", &volume);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().SetBGMVolume(volume);
    }

    return mrb_nil_value();
}

static mrb_value rb_audio_set_se_volume(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_float volume;
    mrb_get_args(mrb, "f", &volume);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (engine) {
        engine->GetAudio().SetSEVolume(volume);
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
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(4)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "se_play",
        rb_audio_play_sound,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "bgs_play",
        rb_audio_play_bgs,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(4)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "me_play",
        rb_audio_play_me,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2)
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
        "bgs_stop",
        rb_audio_stop_bgs,
        MRB_ARGS_NONE()
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "fade_out",
        rb_audio_fade_out,
        MRB_ARGS_OPT(1)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "volume=",
        rb_audio_set_volume,
        MRB_ARGS_REQ(1)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "bgm_volume=",
        rb_audio_set_bgm_volume,
        MRB_ARGS_REQ(1)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "se_volume=",
        rb_audio_set_se_volume,
        MRB_ARGS_REQ(1)
    );

    // Aliases for compatibility
    mrb_define_module_function(
        mMrb,
        audioModule,
        "play_music",
        rb_audio_play_music,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(4)
    );

    mrb_define_module_function(
        mMrb,
        audioModule,
        "play_sound",
        rb_audio_play_sound,
        MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2)
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

// ==================== UI / ScreenText Bindings ====================

static mrb_value rb_ui_show_message(mrb_state* mrb, mrb_value self) {
    (void)self;
    char* text = nullptr;
    mrb_get_args(mrb, "z", &text);
    if (text) {
        GameUI::Get().ShowMessage(std::string(text));
    }
    return mrb_nil_value();
}

static mrb_value rb_ui_show_screen_text(mrb_state* mrb, mrb_value self) {
    (void)self;
    char* text = nullptr;
    mrb_float x = 0.5f, y = 0.1f;
    mrb_float r = 1.0f, g = 1.0f, b = 1.0f;
    mrb_float duration = 3.0f;
    mrb_get_args(mrb, "z|fffff", &text, &x, &y, &r, &g, &b);
    // Try to parse optional duration as 6th arg if provided via extra handling
    // For simplicity, we use 5 args; if caller provides more, we ignore
    if (!text) return mrb_nil_value();
    // If more args passed, try to get duration via checking stack?
    // We'll attempt to get extra float for duration from optional
    // Actually MRB_ARGS allows us to parse flexibly - we already handle x,y,r,g,b
    // Duration will be parsed as extra if provided: we can attempt another parse
    int argc = mrb_get_argc(mrb);
    if (argc >= 6) {
        // Re-parse with duration
        mrb_get_args(mrb, "z|ffffff", &text, &x, &y, &r, &g, &b, &duration);
    }
    int id = GameUI::Get().AddScreenText(std::string(text), Vec2((float)x, (float)y), Color((float)r, (float)g, (float)b, 1.0f), (float)duration);
    return mrb_int_value(mrb, id);
}

static mrb_value rb_ui_show_world_text(mrb_state* mrb, mrb_value self) {
    (void)self;
    char* text = nullptr;
    mrb_float x = 0, y = 0, z = 0;
    mrb_float r = 1.0f, g = 1.0f, b = 0.0f;
    mrb_float duration = 2.5f;
    mrb_get_args(mrb, "z|ffffff", &text, &x, &y, &z, &r, &g, &b);
    int argc = mrb_get_argc(mrb);
    if (argc >= 7) {
        mrb_get_args(mrb, "z|fffffff", &text, &x, &y, &z, &r, &g, &b, &duration);
    }
    if (!text) return mrb_nil_value();
    int id = GameUI::Get().AddWorldText(std::string(text), Vec3((float)x, (float)y, (float)z), Color((float)r, (float)g, (float)b, 1.0f), (float)duration);
    return mrb_int_value(mrb, id);
}

static mrb_value rb_ui_clear_screen_texts(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    GameUI::Get().ClearScreenTexts();
    return mrb_nil_value();
}

static mrb_value rb_ui_gold(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, Game::Get().Party().GetGold());
}

static mrb_value rb_ui_add_gold(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int amount;
    mrb_get_args(mrb, "i", &amount);
    Game::Get().Party().GainGold((int)amount);
    return mrb_nil_value();
}

static mrb_value rb_ui_show_picture(mrb_state* mrb, mrb_value self) {
    (void)self;
    char* filename = nullptr;
    char* name = nullptr;
    mrb_float x = 0.5f, y = 0.5f;
    mrb_float scale = 1.0f, opacity = 1.0f;
    // Args: filename, name (optional), x, y, scale, opacity
    mrb_get_args(mrb, "z|zffff", &filename, &name, &x, &y, &scale, &opacity);
    if (!filename) return mrb_nil_value();
    std::string fname(filename);
    std::string picName = name ? std::string(name) : fname;
    int id = GameUI::Get().ShowPicture(fname, picName, Vec2((float)x, (float)y), (float)scale, (float)opacity, 0.0f);
    return mrb_int_value(mrb, id);
}

static mrb_value rb_ui_move_picture(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value id_or_name;
    mrb_float x = 0.5f, y = 0.5f;
    mrb_float duration = 0.5f;
    mrb_int easing = 2;
    mrb_get_args(mrb, "o|fffi", &id_or_name, &x, &y, &duration, &easing);
    if (mrb_string_p(id_or_name)) {
        std::string name(mrb_string_value_cstr(mrb, &id_or_name));
        GameUI::Get().MovePicture(name, Vec2((float)x, (float)y), (float)duration, (int)easing);
    } else if (mrb_int_p(id_or_name)) {
        int id = (int)mrb_integer(id_or_name);
        GameUI::Get().MovePicture(id, Vec2((float)x, (float)y), (float)duration, (int)easing);
    }
    return mrb_nil_value();
}

static mrb_value rb_ui_tween_picture(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value id_val;
    mrb_float x = 0.5f, y = 0.5f, scale = 1.0f, opacity = 1.0f, rotation = 0.0f;
    mrb_float duration = 1.0f;
    mrb_int easing = 2;
    mrb_get_args(mrb, "o|ffffffi", &id_val, &x, &y, &scale, &opacity, &rotation, &duration, &easing);
    int id = 0;
    if (mrb_int_p(id_val)) id = (int)mrb_integer(id_val);
    else return mrb_nil_value();
    GameUI::Get().TweenPicture(id, Vec2((float)x, (float)y), (float)scale, (float)opacity, (float)rotation, (float)duration, (int)easing);
    return mrb_nil_value();
}

static mrb_value rb_ui_remove_picture(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value id_or_name;
    mrb_get_args(mrb, "o", &id_or_name);
    if (mrb_string_p(id_or_name)) {
        std::string name(mrb_string_value_cstr(mrb, &id_or_name));
        GameUI::Get().RemovePicture(name);
    } else if (mrb_int_p(id_or_name)) {
        int id = (int)mrb_integer(id_or_name);
        GameUI::Get().RemovePicture(id);
    } else {
        GameUI::Get().ClearPictures();
    }
    return mrb_nil_value();
}

static mrb_value rb_game_map_visible(mrb_state* mrb, mrb_value self) {
    (void)self;
    (void)mrb;
    return mrb_bool_value(Game::Get().Map().IsVisible());
}

static mrb_value rb_game_map_set_visible(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_bool visible;
    mrb_get_args(mrb, "b", &visible);
    Game::Get().Map().SetVisible(visible);
    return mrb_nil_value();
}

static mrb_value rb_game_map_id(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, Game::Get().Map().GetMapId());
}

static mrb_value rb_game_map_setup(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int mapId;
    mrb_get_args(mrb, "i", &mapId);
    Game::Get().Map().Setup((int)mapId);
    return mrb_nil_value();
}


// --- RPG Maker Kern: Save / Load / Battle / Switches / Variables ---
static mrb_value rb_game_save(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int slot = 1;
    mrb_get_args(mrb, "|i", &slot);
    bool ok = Game::Get().Save((int)slot);
    return mrb_bool_value(ok);
}
static mrb_value rb_game_load(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int slot = 1;
    mrb_get_args(mrb, "|i", &slot);
    bool ok = Game::Get().Load((int)slot);
    return mrb_bool_value(ok);
}
static mrb_value rb_game_switch_get(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_bool_value(Game::Get().Switches().Get((int)id));
}
static mrb_value rb_game_switch_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id; mrb_bool val;
    mrb_get_args(mrb, "ib", &id, &val);
    Game::Get().Switches().Set((int)id, val);
    EventSystem::Get().RefreshAllPages(); // XP: $game_map.need_refresh
    return mrb_nil_value();
}
static mrb_value rb_game_var_get(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_int_value(mrb, Game::Get().Variables().Get((int)id));
}
static mrb_value rb_game_var_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id, val;
    mrb_get_args(mrb, "ii", &id, &val);
    Game::Get().Variables().Set((int)id, (int)val);
    EventSystem::Get().RefreshAllPages(); // Bedingungen reagieren sofort
    return mrb_nil_value();
}
static mrb_value rb_battle_start(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int troopId = 1;
    mrb_get_args(mrb, "|i", &troopId);
    std::vector<int> enemies;
    if (const auto* tr = Database::Get().GetTroop((int)troopId))
        enemies = tr->members;
    if (enemies.empty()) enemies = {1};
    BattleSystem::Get().Setup(enemies, true, false);
    BattleSystem::Get().onMessage = [](const std::string& msg) {
        GameUI::Get().ShowMessage(msg);
    };
    BattleAction act; act.type = BattleActionType::Attack;
    BattleSystem::Get().SetAction(act);
    GameUI::Get().ShowMessage("Battle!");
    return mrb_nil_value();
}
static mrb_value rb_battle_in_battle(mrb_state* mrb, mrb_value self) {
    (void)self; (void)mrb;
    return mrb_bool_value(BattleSystem::Get().IsInBattle());
}

// ==================== XP-Spielobjekte (Ruby <-> C++) ====================
// Damit Skripte wie in RPG Maker XP DIREKT auf die Spiel-Daten zugreifen
// koennen ($game_switches[3] = true, $game_party.gold, $game_player.x ...),
// gibt es diese Bridges. Alle Setter loesen EventSystem::RefreshAllPages()
// aus (XP: $game_map.need_refresh), damit Event-Seiten, deren Bedingungen
// auf Switches/Variables/SelfSwitches/Items/Mitglieder zeigen, sofort
// reagieren – genau das macht das Spiel "vom Skript-Editor aus steuerbar".

// ---------- $game_switches (Game_Switches) ----------
static mrb_value rb_gsw_aref(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_bool_value(Game::Get().Switches().Get((int)id));
}
static mrb_value rb_gsw_aset(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id; mrb_bool val;
    mrb_get_args(mrb, "ib", &id, &val);
    Game::Get().Switches().Set((int)id, val);
    EventSystem::Get().RefreshAllPages();
    return mrb_bool_value(val);
}
static mrb_value rb_gsw_size(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, (mrb_int)Game::Get().Switches().Size());
}

// ---------- $game_variables (Game_Variables) ----------
static mrb_value rb_gvar_aref(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_int_value(mrb, (mrb_int)Game::Get().Variables().Get((int)id));
}
static mrb_value rb_gvar_aset(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id, val;
    mrb_get_args(mrb, "ii", &id, &val);
    Game::Get().Variables().Set((int)id, (int)val);
    EventSystem::Get().RefreshAllPages();
    return mrb_int_value(mrb, val);
}
static mrb_value rb_gvar_size(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, (mrb_int)EngineConfig::MAX_VARIABLES);
}

// ---------- $game_self_switches (Game_SelfSwitches, Key = [map, event, "A"]) ----------
static bool ssw_parse_key(mrb_state* mrb, mrb_value key, int& mapId, int& evId, char& ch) {
    if (!mrb_array_p(key)) return false;
    if (RARRAY_LEN(key) < 3) return false;
    mrb_value v0 = mrb_ary_ref(mrb, key, 0);
    mrb_value v1 = mrb_ary_ref(mrb, key, 1);
    mrb_value v2 = mrb_ary_ref(mrb, key, 2);
    if (!(mrb_integer_p(v0) || mrb_float_p(v0))) return false;
    if (!(mrb_integer_p(v1) || mrb_float_p(v1))) return false;
    if (!mrb_string_p(v2) || RSTRING_LEN(v2) < 1) return false;
    mapId = (int)(mrb_integer_p(v0) ? mrb_integer(v0) : (mrb_int)mrb_float(v0));
    evId  = (int)(mrb_integer_p(v1) ? mrb_integer(v1) : (mrb_int)mrb_float(v1));
    ch = RSTRING_PTR(v2)[0];
    return true;
}
static mrb_value rb_gssw_aref(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value key;
    mrb_get_args(mrb, "o", &key);
    int mapId = 0, evId = 0; char ch = 'A';
    if (!ssw_parse_key(mrb, key, mapId, evId, ch)) return mrb_false_value();
    return mrb_bool_value(Game::Get().SelfSwitches().Get(mapId, evId, ch));
}
static mrb_value rb_gssw_aset(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value key; mrb_bool val;
    mrb_get_args(mrb, "ob", &key, &val);
    int mapId = 0, evId = 0; char ch = 'A';
    if (!ssw_parse_key(mrb, key, mapId, evId, ch)) return mrb_bool_value(val);
    Game::Get().SelfSwitches().Set(mapId, evId, ch, val);
    EventSystem::Get().RefreshAllPages();
    return mrb_bool_value(val);
}
static mrb_value rb_gssw_size(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, (mrb_int)Game::Get().SelfSwitches().Data().size());
}

// ---------- $game_party (Game_Party) ----------
static mrb_value rb_gp_gold(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().GetGold());
}
static mrb_value rb_gp_gain_gold(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int n;
    mrb_get_args(mrb, "i", &n);
    Game::Get().Party().GainGold((int)n);
    EventSystem::Get().RefreshAllPages(); // Bedingung "Gold oder mehr"
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().GetGold());
}
static mrb_value rb_gp_lose_gold(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int n;
    mrb_get_args(mrb, "i", &n);
    Game::Get().Party().GainGold(-(int)n);
    EventSystem::Get().RefreshAllPages();
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().GetGold());
}
static mrb_value rb_gp_item_count(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().GetItemCount((int)id));
}
static mrb_value rb_gp_gain_item(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id, n;
    mrb_get_args(mrb, "ii", &id, &n);
    Game::Get().Party().GainItem((int)id, (int)n);
    EventSystem::Get().RefreshAllPages(); // Bedingung "Gegenstand vorhanden"
    return mrb_nil_value();
}
static mrb_value rb_gp_weapon_count(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().GetWeaponCount((int)id));
}
static mrb_value rb_gp_gain_weapon(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id, n;
    mrb_get_args(mrb, "ii", &id, &n);
    Game::Get().Party().GainWeapon((int)id, (int)n);
    return mrb_nil_value();
}
static mrb_value rb_gp_armor_count(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().GetArmorCount((int)id));
}
static mrb_value rb_gp_gain_armor(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id, n;
    mrb_get_args(mrb, "ii", &id, &n);
    Game::Get().Party().GainArmor((int)id, (int)n);
    return mrb_nil_value();
}
static mrb_value rb_gp_has_actor(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    return mrb_bool_value(Game::Get().Party().HasActor((int)id));
}
static mrb_value rb_gp_add_actor(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    Game::Get().Party().AddActor((int)id);
    EventSystem::Get().RefreshAllPages(); // Bedingung "Akteur in der Gruppe"
    return mrb_nil_value();
}
static mrb_value rb_gp_remove_actor(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    Game::Get().Party().RemoveActor((int)id);
    EventSystem::Get().RefreshAllPages();
    return mrb_nil_value();
}
static mrb_value rb_gp_members_size(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_int_value(mrb, (mrb_int)Game::Get().Party().Members().size());
}
static mrb_value rb_gp_members(mrb_state* mrb, mrb_value self) {
    (void)self;
    auto& members = Game::Get().Party().Members();
    mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)members.size());
    for (const auto& a : members) {
        mrb_value h = mrb_hash_new_capa(mrb, 6);
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "id")),
                     mrb_int_value(mrb, (mrb_int)a.actorId));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "name")),
                     mrb_str_new(mrb, a.name.c_str(), (mrb_int)a.name.size()));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "level")),
                     mrb_int_value(mrb, (mrb_int)a.level));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "hp")),
                     mrb_int_value(mrb, (mrb_int)a.hp));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "mp")),
                     mrb_int_value(mrb, (mrb_int)a.mp));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "exp")),
                     mrb_int_value(mrb, (mrb_int)a.exp));
        mrb_ary_push(mrb, ary, h);
    }
    return ary;
}

// ---------- $game_player (Game_Player) ----------
static mrb_value rb_gpl_x(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_float_value(mrb, Game::Get().Player().GetPosition().x);
}
static mrb_value rb_gpl_y(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_float_value(mrb, Game::Get().Player().GetPosition().y);
}
static mrb_value rb_gpl_z(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_float_value(mrb, Game::Get().Player().GetPosition().z);
}
static mrb_value rb_gpl_move_to(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    Game::Get().Player().SetPosition(Vec3((float)x, (float)y, (float)z));
    return mrb_nil_value();
}
static mrb_value rb_gpl_locked(mrb_state* mrb, mrb_value self) {
    (void)self; (void)mrb;
    return mrb_bool_value(Game::Get().Player().IsLocked());
}
static mrb_value rb_gpl_set_locked(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_bool v;
    mrb_get_args(mrb, "b", &v);
    Game::Get().Player().SetLocked(v);
    return mrb_bool_value(v);
}
static mrb_value rb_gpl_moving(mrb_state* mrb, mrb_value self) {
    (void)self; (void)mrb;
    return mrb_bool_value(Game::Get().Player().IsMoving());
}

// ---------- $game_map Zusatz: Breite/Hoehe der geladenen Karte ----------
static mrb_value rb_game_map_width(mrb_state* mrb, mrb_value self) {
    (void)self;
    Engine* e = static_cast<Engine*>(mrb->ud);
    return mrb_int_value(mrb, (e && e->IsInitialized()) ? (mrb_int)e->GetMap().GetWidth() : 0);
}
static mrb_value rb_game_map_height(mrb_state* mrb, mrb_value self) {
    (void)self;
    Engine* e = static_cast<Engine*>(mrb->ud);
    return mrb_int_value(mrb, (e && e->IsInitialized()) ? (mrb_int)e->GetMap().GetHeight() : 0);
}

// ---------- HUD an/aus aus Ruby (UI.hud_visible = true/false) ----------
static mrb_value rb_ui_hud_set_visible(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_bool v;
    mrb_get_args(mrb, "b", &v);
    Engine* e = static_cast<Engine*>(mrb->ud);
    if (e && e->GetRmlUi()) e->GetRmlUi()->SetVisible(v);
    return mrb_bool_value(v);
}
static mrb_value rb_ui_hud_visible(mrb_state* mrb, mrb_value self) {
    (void)self;
    Engine* e = static_cast<Engine*>(mrb->ud);
    return mrb_bool_value(e && e->GetRmlUi() && e->GetRmlUi()->IsVisible());
}

void RubyVM::BindUI() {
    struct RClass* uiModule = mrb_define_module(mMrb, "UI");

    mrb_define_module_function(mMrb, uiModule, "show_message", rb_ui_show_message, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "show_text", rb_ui_show_screen_text, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(6));
    mrb_define_module_function(mMrb, uiModule, "show_screen_text", rb_ui_show_screen_text, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(6));
    mrb_define_module_function(mMrb, uiModule, "show_world_text", rb_ui_show_world_text, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(7));
    mrb_define_module_function(mMrb, uiModule, "clear_texts", rb_ui_clear_screen_texts, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "gold", rb_ui_gold, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "add_gold", rb_ui_add_gold, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "show_picture", rb_ui_show_picture, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(5));
    mrb_define_module_function(mMrb, uiModule, "move_picture", rb_ui_move_picture, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(4));
    mrb_define_module_function(mMrb, uiModule, "tween_picture", rb_ui_tween_picture, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(7));
    mrb_define_module_function(mMrb, uiModule, "remove_picture", rb_ui_remove_picture, MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, uiModule, "clear_pictures", rb_ui_remove_picture, MRB_ARGS_NONE());
    // HUD ein-/ausblenden (zeigt im Game-Fenster HP/MP/Gold/Karte)
    mrb_define_module_function(mMrb, uiModule, "hud_visible=", rb_ui_hud_set_visible, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "hud_visible?", rb_ui_hud_visible, MRB_ARGS_NONE());

    // Game module extensions for convenience.
    // WICHTIG: als KLASSE definieren (nicht Modul), damit die Spiellogik in
    // main.rb ein "class Game ... end" reoeffnen kann. Ein Modul wuerde
    // "TypeError: Game is not a class" ausloesen (siehe main.rb).
    struct RClass* gameModule = mrb_define_class(mMrb, "Game", mMrb->object_class);
    mrb_define_module_function(mMrb, gameModule, "show_message", rb_ui_show_message, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gameModule, "show_screen_text", rb_ui_show_screen_text, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(6));
    mrb_define_module_function(mMrb, gameModule, "show_world_text", rb_ui_show_world_text, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(7));
    mrb_define_module_function(mMrb, gameModule, "show_picture", rb_ui_show_picture, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(5));
    mrb_define_module_function(mMrb, gameModule, "move_picture", rb_ui_move_picture, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(4));
    mrb_define_module_function(mMrb, gameModule, "tween_picture", rb_ui_tween_picture, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(7));
    mrb_define_module_function(mMrb, gameModule, "remove_picture", rb_ui_remove_picture, MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, gameModule, "save", rb_game_save, MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, gameModule, "load", rb_game_load, MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, gameModule, "switch", rb_game_switch_get, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gameModule, "set_switch", rb_game_switch_set, MRB_ARGS_REQ(2));
    mrb_define_module_function(mMrb, gameModule, "variable", rb_game_var_get, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gameModule, "set_variable", rb_game_var_set, MRB_ARGS_REQ(2));
    mrb_define_module_function(mMrb, gameModule, "start_battle", rb_battle_start, MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, gameModule, "in_battle?", rb_battle_in_battle, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gameModule, "map_visible", rb_game_map_visible, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gameModule, "set_map_visible", rb_game_map_set_visible, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gameModule, "map_id", rb_game_map_id, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gameModule, "setup_map", rb_game_map_setup, MRB_ARGS_REQ(1));

    // Game_Map module for map data via script - damit Scenes Map Daten nutzen
    struct RClass* gameMapModule = mrb_define_module(mMrb, "Game_Map");
    mrb_define_module_function(mMrb, gameMapModule, "visible?", rb_game_map_visible, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gameMapModule, "visible=", rb_game_map_set_visible, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gameMapModule, "id", rb_game_map_id, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gameMapModule, "setup", rb_game_map_setup, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gameMapModule, "width", rb_game_map_width, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gameMapModule, "height", rb_game_map_height, MRB_ARGS_NONE());

    // ---------- XP-Spielobjekte als globale Variablen ($game_*) ----------
    // Exakt wie in RPG Maker XP: Skripte schreiben $game_switches[5] = true
    // statt eines umstaendlichen Funktionsaufrufs. Setter refreshen die
    // Event-Seiten sofort (XP: $game_map.need_refresh).
    struct RClass* cSwitches = mrb_define_class(mMrb, "Game_Switches", mMrb->object_class);
    mrb_define_method(mMrb, cSwitches, "[]", rb_gsw_aref, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cSwitches, "[]=", rb_gsw_aset, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cSwitches, "size", rb_gsw_size, MRB_ARGS_NONE());
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_switches"),
               mrb_obj_new(mMrb, cSwitches, 0, nullptr));

    struct RClass* cVariables = mrb_define_class(mMrb, "Game_Variables", mMrb->object_class);
    mrb_define_method(mMrb, cVariables, "[]", rb_gvar_aref, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cVariables, "[]=", rb_gvar_aset, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cVariables, "size", rb_gvar_size, MRB_ARGS_NONE());
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_variables"),
               mrb_obj_new(mMrb, cVariables, 0, nullptr));

    struct RClass* cSelfSwitches = mrb_define_class(mMrb, "Game_SelfSwitches", mMrb->object_class);
    mrb_define_method(mMrb, cSelfSwitches, "[]", rb_gssw_aref, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cSelfSwitches, "[]=", rb_gssw_aset, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cSelfSwitches, "size", rb_gssw_size, MRB_ARGS_NONE());
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_self_switches"),
               mrb_obj_new(mMrb, cSelfSwitches, 0, nullptr));

    struct RClass* cParty = mrb_define_class(mMrb, "Game_Party", mMrb->object_class);
    mrb_define_method(mMrb, cParty, "gold", rb_gp_gold, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cParty, "gain_gold", rb_gp_gain_gold, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "lose_gold", rb_gp_lose_gold, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "item_count", rb_gp_item_count, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "gain_item", rb_gp_gain_item, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cParty, "weapon_count", rb_gp_weapon_count, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "gain_weapon", rb_gp_gain_weapon, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cParty, "armor_count", rb_gp_armor_count, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "gain_armor", rb_gp_gain_armor, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cParty, "has_actor", rb_gp_has_actor, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "add_actor", rb_gp_add_actor, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "remove_actor", rb_gp_remove_actor, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "members_size", rb_gp_members_size, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cParty, "members", rb_gp_members, MRB_ARGS_NONE());
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_party"),
               mrb_obj_new(mMrb, cParty, 0, nullptr));

    struct RClass* cPlayer = mrb_define_class(mMrb, "Game_Player", mMrb->object_class);
    mrb_define_method(mMrb, cPlayer, "x", rb_gpl_x, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cPlayer, "y", rb_gpl_y, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cPlayer, "z", rb_gpl_z, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cPlayer, "move_to", rb_gpl_move_to, MRB_ARGS_REQ(3));
    mrb_define_method(mMrb, cPlayer, "locked?", rb_gpl_locked, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cPlayer, "locked=", rb_gpl_set_locked, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cPlayer, "moving?", rb_gpl_moving, MRB_ARGS_NONE());
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_player"),
               mrb_obj_new(mMrb, cPlayer, 0, nullptr));

    // $game_map zeigt auf das Game_Map-Modul (gleiche Methoden erreichbar)
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_map"),
               mrb_obj_value(gameMapModule));
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

// ==================== No-Ruby-Stubs ====================
// Targets ohne mruby linken dank dieser No-Op-Definitionen sauber.
// WICHTIG: Bei neuen oeffentlichen RubyVM-Methoden hier einen Stub ergaenzen
// (CollectGarbage fehlte einmal -> LNK2019 im Player).
namespace rpg {

RubyVM::RubyVM() = default;
RubyVM::~RubyVM() = default;

bool RubyVM::Initialize(Engine* engine) {
    (void)engine;
    RPG_LOG_WARN("Ruby support not compiled in. Set RPGMAKER3D_ENABLE_RUBY=ON");
    return false;
}

void RubyVM::Shutdown() {}

bool RubyVM::ExecuteString(const std::string& code, const std::string& sourceName) {
    (void)code; (void)sourceName;
    mLastError = "Ruby support not compiled in";
    return false;
}

bool RubyVM::ExecuteFile(const std::string& path) {
    (void)path;
    mLastError = "Ruby support not compiled in";
    return false;
}

bool RubyVM::Update(float deltaTime) {
    (void)deltaTime;
    return false;
}

// Ohne mruby kann die Syntax nicht geprueft werden -> als ok melden.
bool RubyVM::CheckSyntax(const std::string& code, const std::string& sourceName,
                         std::string& errorOut) {
    (void)code; (void)sourceName;
    errorOut.clear();
    return true;
}

void RubyVM::CollectGarbage() {} // ScriptManager ruft das ungeschuetzt

bool RubyVM::CaptureException(const std::string&) { return false; }

void RubyVM::BindEngine() {}
void RubyVM::BindInput() {}
void RubyVM::BindAudio() {}
void RubyVM::BindMap() {}
void RubyVM::BindActor() {}
void RubyVM::BindCamera() {}
void RubyVM::BindGame() {}
void RubyVM::BindUI() {}

} // namespace rpg

#endif // RPGMAKER3D_ENABLE_RUBY
