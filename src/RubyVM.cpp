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
#include "rpgmaker3d/Custom.h" // "alles custom"-Schalter (UI.native_*)
#include "rpgmaker3d/RgssUI.h" // RGSS-Fenstersystem (Ruby-Klasse Window)
#include "rpgmaker3d/Rui.h"    // PAKET 32: eigenes UI-Framework (Script-Windows)

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
#include "rpgmaker3d/RubyCompat.h"

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
#include <cstring> // strlen (mrb_intern mit Laenge, mruby 4.x 3-arg)
#include <iostream>
#include <memory>
#include <algorithm> // std::clamp (MSVC zieht es NICHT transitiv herein)

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
    BindRui();        // PAKET 32: eigenes UI-Framework (Script-Windows)
    BindRgssWindow(); // RGSS: Ruby-Klasse Window (reine Ruby-UI)
    BindRgssObjects();   // RGSS-XP: Rect/Color/Tone/Font/Table/Bitmap/Viewport
    BindRgssDrawables(); // RGSS-XP: Sprite/Plane/Tilemap
    BindRgssGraphics();  // RGSS-XP: Graphics/Input(XP)/Audio(XP)
    BindRgssWindowEx();  // RGSS-XP: Window-Vollset
    LoadRgssPrelude();   // RGSS-XP: RPG::*-Datenklassen, RPG::Cache, ...

    RPG_LOG_INFO("Ruby VM initialized");
    return true;
}

void RubyVM::Shutdown() {
    // In BindUI verdrahteter Hook haelt `this` — VOR mrb_close loesen,
    // sonst zeigt die Game-Lambda spaeter auf einen toten VM-Zeiger.
    Game::Get().onBattleStarted = nullptr;
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

    // 3) XP-Szenen-Framework (PAKET 6/h, Opt-in via UI.xp_scene_mode /
    //    Game.ini XpSceneMode=1): XP treibt Szenen mit einer blockierenden
    //    `while $scene != nil; $scene.main; end`-Schleife — unsere Engine
    //    ownet den Frame-Loop, daher tickt sie stattdessen pro Frame
    //    `$scene.__engine_frame` der Scene_Base (start -> update ->
    //    terminate bei $scene-Wechsel; siehe RgssPrelude). Szenenwechsel
    //    geschieht XP-konform per Zuweisung `$scene = Scene_X.new` und
    //    wird ab dem naechsten Frame wirksam (kein Rekursions-Stapel).
    if (CustomConfig::Get().xpSceneMode) {
        mrb_value scene = mrb_gv_get(mMrb, mrb_intern_lit(mMrb, "$scene"));
        if (!mrb_nil_p(scene)) {
            mrb_funcall(mMrb, scene, "__engine_frame", 0);
            if (mMrb->exc) {
                CaptureException("$scene.__engine_frame");
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Custom-Hooks: die Engine ruft ins Spiel, wenn eingebaute Oberflaechen
// abgeschaltet sind (Game.ini) - z. B. "custom_title" (eigener Titel).
// ---------------------------------------------------------------------------
bool RubyVM::CallGameHook(const std::string& name) {
    if (!mMrb) return false;
    mrb_sym clsSym = mrb_intern_lit(mMrb, "Game");
    if (!mrb_const_defined(mMrb, mrb_obj_value(mMrb->object_class), clsSym)) return false;
    mrb_value gameClass = mrb_const_get(mMrb, mrb_obj_value(mMrb->object_class), clsSym);
    if (mrb_nil_p(gameClass)) return false;
    mrb_sym hook = mrb_intern(mMrb, name.c_str(), (mrb_int)name.size());
    if (!mrb_respond_to(mMrb, gameClass, hook)) return false;
    RPG_LOG_INFO(std::string("[Custom] Ruby-Hook Game.") + name + " wird aufgerufen");
    mrb_funcall_argv(mMrb, gameClass, hook, 0, nullptr);
    if (mMrb->exc) {
        CaptureException("Game." + name);
        // Exception melden, aber der Hook gilt trotzdem als "gefunden"
    }
    return true;
}

void RubyVM::CallListMenuBlock(int index) {
    if (!mMrb) return;
    mrb_sym varSym = mrb_intern_lit(mMrb, "$__rpg3d_menu_block");
    mrb_value blk = mrb_gv_get(mMrb, varSym);
    if (mrb_nil_p(blk)) return;
    mrb_value arg = mrb_int_value(mMrb, index);
    mrb_funcall_argv(mMrb, blk, mrb_intern_lit(mMrb, "call"), 1, &arg);
    if (mMrb->exc) {
        CaptureException("UI.open_list_menu-Block");
        mMrb->exc = nullptr; // UI darf bei Ruby-Fehler nicht stehen bleiben
    }
}

void RubyVM::CallNameInputResult(const std::string& name) {
    if (!mMrb) return;
    mrb_value blk = mrb_gv_get(mMrb, mrb_intern_lit(mMrb, "$__rpg3d_nameinput_block"));
    if (mrb_nil_p(blk)) return;
    mrb_value arg = mrb_str_new(mMrb, name.data(), (mrb_int)name.size());
    mrb_funcall_argv(mMrb, blk, mrb_intern_lit(mMrb, "call"), 1, &arg);
    if (mMrb->exc) {
        CaptureException("UI.open_name_input-Block");
        mMrb->exc = nullptr; // UI darf bei Ruby-Fehler nicht stehen bleiben
    }
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

static Key KeyFromSymbol(mrb_state* mrb, mrb_sym keySymbol) {
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
    return key;
}

static mrb_value rb_input_key_down(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_sym keySymbol;
    mrb_get_args(mrb, "n", &keySymbol);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_bool_value(false);
    }

    return mrb_bool_value(engine->GetInput().IsKeyDown(KeyFromSymbol(mrb, keySymbol)));
}

// Kantenabfrage (genau EINMAL beim Druecken) - fuer Menue-Navigation in
// Custom-Szenen; key_down? bleibt fuer gehaltene Bewegung (Laufen etc.)
static mrb_value rb_input_key_pressed(mrb_state* mrb, mrb_value self) {
    (void)self;

    mrb_sym keySymbol;
    mrb_get_args(mrb, "n", &keySymbol);

    Engine* engine = static_cast<Engine*>(mrb->ud);

    if (!engine) {
        return mrb_bool_value(false);
    }

    return mrb_bool_value(engine->GetInput().IsKeyPressed(KeyFromSymbol(mrb, keySymbol)));
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

    // Kantenabfrage: genau EINMAL true beim Druecken (Menue-Navigation)
    mrb_define_module_function(
        mMrb,
        inputModule,
        "key_pressed?",
        rb_input_key_pressed,
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
    // Gemeinsame Implementierung (auch Player --battletest): Trupp aus der
    // Datenbank, Flucht erlaubt (XP-Standard beim Skriptaufruf).
    Game::Get().StartBattleByTroop((int)troopId, true);
    return mrb_nil_value();
}
static mrb_value rb_battle_in_battle(mrb_state* mrb, mrb_value self) {
    (void)self; (void)mrb;
    return mrb_bool_value(BattleSystem::Get().IsInBattle());
}

// ==================== Battle-Modul (Custom-Kampfszenen) ====================
// Das eingebaute Kampfmenue laesst sich abschalten (Game.ini NativeBattleMenu=0
// oder UI.native_battle_menu = false); eine eigene Ruby-Szene liest dann den
// Zustand ueber Battle.* und gibt Aktionen mit Battle.set_action zurueck.
// Der C++-Kern (Reihenfolge, Schaden, Sieg/Niederlage, EXP) laeuft weiter.

// Hash-Keys als Strings (e["hp"]) - vermeidet Symbol-API, Ruby-seitig simpel.
static void hset(mrb_state* mrb, mrb_value h, const char* key, mrb_value v) {
    mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, key), v);
}
static mrb_value battler_hash(mrb_state* mrb, const Battler& b) {
    mrb_value h = mrb_hash_new_capa(mrb, 11);
    hset(mrb, h, "id",     mrb_int_value(mrb, b.id));
    hset(mrb, h, "index",  mrb_int_value(mrb, b.index));
    hset(mrb, h, "name",   mrb_str_new(mrb, b.name.data(), (mrb_int)b.name.size()));
    hset(mrb, h, "hp",     mrb_int_value(mrb, b.hp));
    hset(mrb, h, "max_hp", mrb_int_value(mrb, b.maxHp));
    hset(mrb, h, "mp",     mrb_int_value(mrb, b.mp));
    hset(mrb, h, "max_mp", mrb_int_value(mrb, b.maxMp));
    hset(mrb, h, "atk",    mrb_int_value(mrb, b.atk));
    hset(mrb, h, "def",    mrb_int_value(mrb, b.def));
    hset(mrb, h, "agi",    mrb_int_value(mrb, b.agi));
    hset(mrb, h, "dead",   mrb_bool_value(b.isDead));
    return h;
}
static mrb_value battlers_array(mrb_state* mrb, std::vector<Battler>& list) {
    mrb_value arr = mrb_ary_new_capa(mrb, (mrb_int)list.size());
    for (const auto& b : list) mrb_ary_push(mrb, arr, battler_hash(mrb, b));
    return arr;
}

// Battle.setup([enemyIds], can_escape=true, can_lose=false) - eigener Kampf
// OHNE Trupp (beliebige Gegnerliste, z. B. aus einer Custom-Szene heraus)
static mrb_value rb_bm_setup(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value ids; mrb_bool canEscape = 1, canLose = 0;
    mrb_get_args(mrb, "o|bb", &ids, &canEscape, &canLose);
    std::vector<int> enemyIds;
    if (mrb_array_p(ids)) {
        const mrb_int n = RARRAY_LEN(ids);
        for (mrb_int i = 0; i < n; ++i) {
            mrb_value v = mrb_ary_ref(mrb, ids, i);
            if (mrb_integer_p(v)) enemyIds.push_back((int)mrb_integer(v));
        }
    } else if (mrb_integer_p(ids)) {
        enemyIds.push_back((int)mrb_integer(ids));
    }
    if (enemyIds.empty()) enemyIds = {1};
    BattleSystem::Get().Setup(enemyIds, canEscape != 0, canLose != 0);
    BattleSystem::Get().onMessage = [](const std::string& m) {
        GameUI::Get().ShowMessage(m);
    };
    return mrb_nil_value();
}
static mrb_value rb_bm_needs_input(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_bool_value(BattleSystem::Get().NeedsInput());
}
static mrb_value rb_bm_input_actor_index(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_int_value(mrb, BattleSystem::Get().NeedsInput()
        ? BattleSystem::Get().GetInputActorIndex() : -1);
}
static mrb_value rb_bm_input_actor_id(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    auto& bs = BattleSystem::Get();
    if (!bs.NeedsInput()) return mrb_int_value(mrb, 0);
    const int idx = bs.GetInputActorIndex();
    if (idx < 0 || idx >= (int)bs.Actors().size()) return mrb_int_value(mrb, 0);
    return mrb_int_value(mrb, bs.Actors()[(size_t)idx].id);
}
static mrb_value rb_bm_can_escape(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_bool_value(BattleSystem::Get().CanEscape());
}
static mrb_value rb_bm_turn(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_int_value(mrb, BattleSystem::Get().GetTurn());
}
static mrb_value rb_bm_last_outcome(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_int_value(mrb, BattleSystem::Get().GetLastOutcome());
}
static mrb_value rb_bm_last_exp(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_int_value(mrb, BattleSystem::Get().LastExp());
}
static mrb_value rb_bm_last_gold(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_int_value(mrb, BattleSystem::Get().LastGold());
}
static mrb_value rb_bm_actors(mrb_state* mrb, mrb_value self) {
    (void)self;
    return battlers_array(mrb, BattleSystem::Get().Actors());
}
static mrb_value rb_bm_enemies(mrb_state* mrb, mrb_value self) {
    (void)self;
    return battlers_array(mrb, BattleSystem::Get().Enemies());
}
// Battle.set_action(Battle::ATTACK, target_index=0, skill_id=0, item_id=0,
//                   target_is_actor=false) - Antwort auf needs_input?
static mrb_value rb_bm_set_action(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int type = 0, targetIndex = 0, skillId = 0, itemId = 0;
    mrb_bool targetIsActor = 0;
    mrb_get_args(mrb, "i|iiiib", &type, &targetIndex, &skillId, &itemId, &targetIsActor);
    BattleAction act;
    act.type = (BattleActionType)(int)type;
    act.subjectIndex = BattleSystem::Get().GetInputActorIndex();
    act.targetIndex = (int)targetIndex;
    act.skillId = (int)skillId;
    act.itemId = (int)itemId;
    act.targetIsActor = (targetIsActor != 0);
    BattleSystem::Get().SetAction(act);
    return mrb_nil_value();
}
static mrb_value rb_bm_abort(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    BattleSystem::Get().Abort();
    return mrb_nil_value();
}
// Direkte Wert-Aenderung fuer Custom-Aktionen ausserhalb von set_action
// (Sieg/Niederlage dabei selbst via "dead"-Flags auswerten)
static mrb_value rb_bm_damage_enemy(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int idx, dmg;
    mrb_get_args(mrb, "ii", &idx, &dmg);
    auto& list = BattleSystem::Get().Enemies();
    if (idx >= 0 && idx < (mrb_int)list.size()) list[(size_t)idx].ApplyDamage((int)dmg);
    return mrb_nil_value();
}
static mrb_value rb_bm_damage_actor(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int idx, dmg;
    mrb_get_args(mrb, "ii", &idx, &dmg);
    auto& list = BattleSystem::Get().Actors();
    if (idx >= 0 && idx < (mrb_int)list.size()) list[(size_t)idx].ApplyDamage((int)dmg);
    return mrb_nil_value();
}
static mrb_value rb_bm_heal_enemy(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int idx, hp, mp = 0;
    mrb_get_args(mrb, "ii|i", &idx, &hp, &mp);
    auto& list = BattleSystem::Get().Enemies();
    if (idx >= 0 && idx < (mrb_int)list.size()) list[(size_t)idx].Recover((int)hp, (int)mp);
    return mrb_nil_value();
}
static mrb_value rb_bm_heal_actor(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int idx, hp, mp = 0;
    mrb_get_args(mrb, "ii|i", &idx, &hp, &mp);
    auto& list = BattleSystem::Get().Actors();
    if (idx >= 0 && idx < (mrb_int)list.size()) list[(size_t)idx].Recover((int)hp, (int)mp);
    return mrb_nil_value();
}
static mrb_value rb_bm_message(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value text;
    mrb_get_args(mrb, "o", &text);
    if (mrb_string_p(text)) {
        const std::string s(RSTRING_PTR(text), (size_t)RSTRING_LEN(text));
        if (BattleSystem::Get().onMessage) BattleSystem::Get().onMessage(s);
        else GameUI::Get().ShowMessage(s);
    }
    return mrb_nil_value();
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

// ---------- XP Game_Party-Zusaetze (Stufe 4g): Identitaets-IDs + XP-Namen
static mrb_value rb_gp_actor_ids(mrb_state* mrb, mrb_value self) {
    (void)self;
    const auto& members = Game::Get().Party().Members();
    mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)members.size());
    for (const auto& a : members)
        mrb_ary_push(mrb, ary, mrb_int_value(mrb, (mrb_int)a.actorId));
    return ary;
}
static mrb_value rb_gp_all_dead(mrb_state* mrb, mrb_value self) {
    (void)self;
    const auto& members = Game::Get().Party().Members();
    if (members.empty()) return mrb_false_value();
    for (const auto& a : members) if (!a.IsDead()) return mrb_false_value();
    return mrb_true_value();
}
static mrb_value GpIdList(mrb_state* mrb, const std::unordered_map<int, int>& m) {
    mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)m.size());
    for (const auto& kv : m)
        if (kv.second > 0) mrb_ary_push(mrb, ary, mrb_int_value(mrb, (mrb_int)kv.first));
    return ary;
}
static mrb_value rb_gp_item_ids(mrb_state* mrb, mrb_value self) {
    (void)self;
    return GpIdList(mrb, Game::Get().Party().Items());
}
static mrb_value rb_gp_weapon_ids(mrb_state* mrb, mrb_value self) {
    (void)self;
    return GpIdList(mrb, Game::Get().Party().Weapons());
}
static mrb_value rb_gp_armor_ids(mrb_state* mrb, mrb_value self) {
    (void)self;
    return GpIdList(mrb, Game::Get().Party().Armors());
}
static mrb_value rb_gp_has_item(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int id = 0;
    mrb_get_args(mrb, "i", &id);
    return mrb_bool_value(Game::Get().Party().GetItemCount((int)id) > 0);
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

// ---------- XP Game_Map-Flags (PAKET 6, XP_Scripts): passable?/bush?/terrain_tag ----------
// Greifen auf die TilesetData-Tabellen von Paket 1 zu (gleiche Semantik wie
// die Laufzeit-Kollision). Signatur wie XP: passable?(x, y, d = 0, self_event = nil)
static mrb_value rb_game_map_passable(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int x = 0, y = 0, d = 0;
    mrb_value selfEv;
    mrb_get_args(mrb, "ii|io", &x, &y, &d, &selfEv);
    // XP-Richtung d -> Paket-1-DirBits (1=unten,2=links,4=rechts,8=oben)
    int dirBit = 0;
    if (d == 2) dirBit = 1;      // unten
    else if (d == 4) dirBit = 2; // links
    else if (d == 6) dirBit = 4; // rechts
    else if (d == 8) dirBit = 8; // oben
    return mrb_bool_value(Game::Get().Map().IsPassable((int)x, (int)y, dirBit));
}
static mrb_value rb_game_map_bush(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int x = 0, y = 0;
    mrb_get_args(mrb, "ii", &x, &y);
    return mrb_bool_value(Game::Get().Map().IsBushAt((int)x, (int)y));
}
static mrb_value rb_game_map_terrain_tag(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int x = 0, y = 0;
    mrb_get_args(mrb, "ii", &x, &y);
    return mrb_int_value(mrb, Game::Get().Map().GetTerrainTagAt((int)x, (int)y));
}

// ---------- XP Game_Map-Instanz (Stufe 4f): data/display/refresh/events ----------
// data: baut bei jedem Aufruf frisch eine Table(w, h, 3) aus der gebundenen
// Karte (XP liest $game_map.data[x, y, z]; unsere nativen IDs -> +384
// RGSS-Offset). Schreib-Zuruecknehmen bewusst nicht (ehrliche Grenze).
static mrb_value rb_game_map_data(mrb_state* mrb, mrb_value self) {
    (void)self;
    const Map* bound = Game::Get().Map().GetBoundMap();
    const int w = bound ? bound->GetWidth() : 0;
    const int h = bound ? bound->GetHeight() : 0;
    if (w <= 0 || h <= 0) return mrb_nil_value();
    struct RClass* tcls = mrb_class_get(mrb, "Table");
    mrb_value dims[3] = { mrb_int_value(mrb, w), mrb_int_value(mrb, h),
                          mrb_int_value(mrb, 3) };
    mrb_value t = mrb_obj_new(mrb, tcls, 3, dims);
    mrb_value idv = mrb_iv_get(mrb, t, mrb_intern_lit(mrb, "__rgss_table_id"));
    if (!mrb_int_p(idv)) return t; // defensiv: leere Table zurueckgeben
    RgssTableState* state = RgssTableGet((int)mrb_as_int(mrb, idv));
    if (!state) return t;
    const auto& layers = bound->GetLayers();
    for (size_t li = 0; li < layers.size() && li < 3; ++li) {
        const auto& lay = layers[li];
        for (int z = 0; z < h; ++z) {
            for (int x = 0; x < w; ++x) {
                int v = 0;
                if (x < lay.width && z < lay.height) {
                    const int idx = z * lay.width + x;
                    if (idx >= 0 && idx < (int)lay.tiles.size()) {
                        const int tId = lay.tiles[idx];
                        v = tId < 0 ? 0 : tId + 384; // native -> XP-RGSS
                    }
                }
                state->data[(((size_t)li * (size_t)h) + (size_t)z) * (size_t)w + (size_t)x] =
                    (int16_t)v;
            }
        }
    }
    return t;
}

// display_x/display_y in XP-Einheit (1/128 Kachel). Abbildung auf den
// logischen Scroll-State GameMap::mDisplayPos (Kacheleinheiten) — das
// Kamera-Verhalten selbst bleibt unveraendert (ehrliche Grenze).
static mrb_value rb_game_map_display_x_get(mrb_state* mrb, mrb_value self) {
    (void)self;
    const double v = (double)Game::Get().Map().GetDisplayPos().x * 128.0;
    return mrb_int_value(mrb, (mrb_int)(v >= 0 ? v + 0.5 : v - 0.5));
}
static mrb_value rb_game_map_display_x_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    Vec3 p = Game::Get().Map().GetDisplayPos();
    p.x = (float)((double)v / 128.0);
    Game::Get().Map().SetDisplayPos(p);
    return mrb_int_value(mrb, v);
}
static mrb_value rb_game_map_display_y_get(mrb_state* mrb, mrb_value self) {
    (void)self;
    // x/z-Ebene: XP display_y bildet auf unsere z-Kachelkoordinate ab
    const double v = (double)Game::Get().Map().GetDisplayPos().z * 128.0;
    return mrb_int_value(mrb, (mrb_int)(v >= 0 ? v + 0.5 : v - 0.5));
}
static mrb_value rb_game_map_display_y_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    Vec3 p = Game::Get().Map().GetDisplayPos();
    p.z = (float)((double)v / 128.0);
    Game::Get().Map().SetDisplayPos(p);
    return mrb_int_value(mrb, v);
}

// PAKET 19: XP $game_map.events — {event_id => Game_Event} der AKTUELLEN
// Karte. Die Objekte sind dauerhaft gecacht (XP: gleiche Instanzen, damit
// Skripte Ivars an ihnen setzen koennen); bei Kartenwechsel (map_id anders)
// wird der Hash neu gebaut. Erased-Events bleiben drin (XP setzt dort nur
// @erased — Spriteset_Map iteriert weiter gefahrlos).
static mrb_value rb_game_map_events(mrb_state* mrb, mrb_value self) {
    const int curMap = Game::Get().Map().GetMapId();
    mrb_value cache = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "__events_cache"));
    mrb_value cacheMap = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "__events_map_id"));
    if (mrb_hash_p(cache) && mrb_int_p(cacheMap) &&
        (int)mrb_as_int(mrb, cacheMap) == curMap)
        return cache;
    struct RClass* cev = mrb_class_get(mrb, "Game_Event");
    mrb_value h = mrb_hash_new(mrb);
    for (const auto& ev : EventSystem::Get().GetEvents()) {
        if (!ev.IsValid()) continue; // Platzhalter ohne Seiten ueberspringen
        mrb_value args[2] = { mrb_int_value(mrb, curMap), mrb_int_value(mrb, ev.id) };
        mrb_value obj = mrb_obj_new(mrb, cev, 2, args);
        mrb_hash_set(mrb, h, mrb_int_value(mrb, ev.id), obj);
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__events_cache"), h);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__events_map_id"),
               mrb_int_value(mrb, curMap));
    return h;
}

// need_refresh: XP-Marker „Events neu bewerten" — bei uns sofort-Semantik:
// setzen auf true fuehrt RefreshAllPages sofort aus (gleiches Muster wie
// die Schalter-Setter), Getter meldet immer false (nie ausstehend).
static mrb_value rb_game_map_need_refresh_get(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_false_value();
}
static mrb_value rb_game_map_need_refresh_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_bool v = 0;
    mrb_get_args(mrb, "b", &v);
    if (v) EventSystem::Get().RefreshAllPages();
    return mrb_bool_value(v);
}
static mrb_value rb_game_map_refresh(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    EventSystem::Get().RefreshAllPages();
    return mrb_nil_value();
}

// ---------------------------------------------------------------------------
// PAKET 19: XP Game_Event-Bruecke — Wrapper auf die nativen MapEvents des
// EventSystems. wie Game_Actor: nur die ID als Ivar, Aufloesung frisch je
// Aufruf (vektorstabil, kartenwechselfest). Events anderer Karten loesen zu
// nullptr auf; alle Methoden liefern dann nil (statt Absturz — ehrlich).
// ---------------------------------------------------------------------------
static MapEvent* GEventPtr(mrb_state* mrb, mrb_value self) {
    const mrb_int id = mrb_as_int(mrb, mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@ev_id")));
    return EventSystem::Get().GetEvent((int)id);
}
static mrb_value rb_gev_initialize(mrb_state* mrb, mrb_value self) {
    mrb_int mapId = 0, evId = 0;
    mrb_get_args(mrb, "|ii", &mapId, &evId);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@map_id"), mrb_int_value(mrb, mapId));
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@ev_id"), mrb_int_value(mrb, evId));
    return self;
}
static mrb_value rb_gev_map_id(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@map_id"));
}
static mrb_value rb_gev_id(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@ev_id"));
}
static mrb_value rb_gev_valid(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return mrb_bool_value(ev && ev->IsValid() && !ev->erased);
}
static mrb_value rb_gev_name(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    if (!ev) return mrb_nil_value();
    return mrb_str_new(mrb, ev->name.data(), (mrb_int)ev->name.size());
}
// XP 2D: x/y — unser grid ist ev.x / ev.z
static mrb_value rb_gev_x(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_int_value(mrb, ev->x) : mrb_nil_value();
}
static mrb_value rb_gev_y(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_int_value(mrb, ev->z) : mrb_nil_value();
}
static mrb_value rb_gev_direction(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_int_value(mrb, ev->direction) : mrb_nil_value();
}
// Laufzeit-Flags aus PAKET 16 (Move-Routen teilen sich die Felder!)
static mrb_value rb_gev_through_get(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_bool_value(ev->through) : mrb_nil_value();
}
static mrb_value rb_gev_through_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = 0;
    mrb_get_args(mrb, "b", &v);
    if (MapEvent* ev = GEventPtr(mrb, self)) ev->through = v != 0;
    return mrb_bool_value(v);
}
static mrb_value rb_gev_transparent_get(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_bool_value(ev->transparent) : mrb_nil_value();
}
static mrb_value rb_gev_transparent_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = 0;
    mrb_get_args(mrb, "b", &v);
    if (MapEvent* ev = GEventPtr(mrb, self)) ev->transparent = v != 0;
    return mrb_bool_value(v);
}
static mrb_value rb_gev_move_speed_get(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_int_value(mrb, ev->moveSpeedRt) : mrb_nil_value();
}
static mrb_value rb_gev_move_speed_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 3;
    mrb_get_args(mrb, "i", &v);
    if (MapEvent* ev = GEventPtr(mrb, self))
        ev->moveSpeedRt = std::clamp((int)v, 1, 6);
    return mrb_int_value(mrb, v);
}
// XP Game_Character#moveto — gleiche Semantik wie Event-Befehl 202
static mrb_value rb_gev_moveto(mrb_state* mrb, mrb_value self) {
    mrb_int x = 0, y = 0;
    mrb_get_args(mrb, "ii", &x, &y);
    if (MapEvent* ev = GEventPtr(mrb, self)) {
        ev->x = (int)x; ev->z = (int)y;
        ev->worldPos = Vec3((float)x, ev->worldPos.y, (float)y);
        ev->direction = DIR_DOWN; // XP: moveto setzt Blick nach unten
    }
    return mrb_nil_value();
}
static mrb_value rb_gev_erase(mrb_state* mrb, mrb_value self) {
    if (MapEvent* ev = GEventPtr(mrb, self)) ev->erased = true;
    return mrb_nil_value();
}
static mrb_value rb_gev_erased(mrb_state* mrb, mrb_value self) {
    MapEvent* ev = GEventPtr(mrb, self);
    return ev ? mrb_bool_value(ev->erased) : mrb_true_value();
}
static mrb_value rb_gev_refresh(mrb_state* mrb, mrb_value self) {
    (void)self;
    EventSystem::Get().RefreshAllPages();
    return mrb_nil_value();
}

// ---------- XP Game_Actor-Bruecke (PAKET 6, XP_Scripts Stufe 4g) ----------
// Wrappt die native GameActor-Laufzeitstruktur (Party-Member). Nicht-Party-
// Akteure bekommen eine fluechtige Setup-Instanz aus der Datenbank (ehrliche
// Naeherung — XP haelt sie persistent, bei uns zaehlt die Party als Quelle).
static GameActor* EngineGameActorFor(int actorId) {
    if (auto* a = Game::Get().Party().GetActor(actorId)) return a;
    static std::unordered_map<int, GameActor> s_orphans;
    auto it = s_orphans.find(actorId);
    if (it != s_orphans.end()) return &it->second;
    GameActor fresh;
    fresh.Setup(actorId);
    auto res = s_orphans.emplace(actorId, std::move(fresh));
    return &res.first->second;
}

static mrb_int GActorId(mrb_state* mrb, mrb_value self) {
    return mrb_as_int(mrb, mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@actor_id")));
}

static mrb_value rb_gactor_initialize(mrb_state* mrb, mrb_value self) {
    mrb_int id = 1;
    mrb_get_args(mrb, "|i", &id);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@actor_id"), mrb_int_value(mrb, id));
    return self;
}
static mrb_value rb_gactor_id(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorId(mrb, self));
}
static GameActor* GActorPtr(mrb_state* mrb, mrb_value self) {
    return EngineGameActorFor((int)GActorId(mrb, self));
}
static mrb_value rb_gactor_exist(mrb_state* mrb, mrb_value self) {
    const bool inParty = Game::Get().Party().GetActor((int)GActorId(mrb, self)) != nullptr;
    return mrb_bool_value(inParty);
}
static mrb_value rb_gactor_name(mrb_state* mrb, mrb_value self) {
    const auto* a = GActorPtr(mrb, self);
    return mrb_str_new(mrb, a->name.data(), (mrb_int)a->name.size());
}
static mrb_value rb_gactor_name_set(mrb_state* mrb, mrb_value self) {
    char* s = nullptr;
    mrb_get_args(mrb, "z", &s);
    auto* a = GActorPtr(mrb, self);
    a->name = s ? s : "";
    return mrb_str_new(mrb, a->name.data(), (mrb_int)a->name.size());
}
static mrb_value rb_gactor_class_id(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->classId);
}
static mrb_value rb_gactor_level(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->level);
}
static mrb_value rb_gactor_level_set(mrb_state* mrb, mrb_value self) {
    mrb_int lv = 1;
    mrb_get_args(mrb, "i", &lv);
    auto* a = GActorPtr(mrb, self);
    const int clamped = (int)std::clamp<mrb_int>(lv, 1, 99);
    a->level = clamped;
    // XP-Annahme: Level wird gesetzt ohne Skill-Nachlernen (Skripte regeln
    // das bei Bedarf ueber learn_skill / Exp-Wege).
    return mrb_int_value(mrb, clamped);
}
static mrb_value rb_gactor_exp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->exp);
}
static mrb_value rb_gactor_exp_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    GActorPtr(mrb, self)->exp = (int)v;
    return mrb_int_value(mrb, v);
}
static mrb_value rb_gactor_next_exp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->ExpForNextLevel());
}
static mrb_value rb_gactor_add_exp(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    return mrb_int_value(mrb, GActorPtr(mrb, self)->AddExp((int)v));
}
static mrb_value rb_gactor_hp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->hp);
}
static mrb_value rb_gactor_hp_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    auto* a = GActorPtr(mrb, self);
    a->hp = std::clamp((int)v, 0, a->MaxHp()); // XP-Clamp
    return mrb_int_value(mrb, a->hp);
}
static mrb_value rb_gactor_sp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->mp);
}
static mrb_value rb_gactor_sp_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    auto* a = GActorPtr(mrb, self);
    a->mp = std::clamp((int)v, 0, a->MaxMp());
    return mrb_int_value(mrb, a->mp);
}
static mrb_value rb_gactor_maxhp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->MaxHp());
}
static mrb_value rb_gactor_maxsp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->MaxMp());
}
static mrb_value rb_gactor_atk(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->Atk());
}
static mrb_value rb_gactor_def(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->Def());
}
static mrb_value rb_gactor_agi(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->Agi());
}
static mrb_value rb_gactor_dead(mrb_state* mrb, mrb_value self) {
    return mrb_bool_value(GActorPtr(mrb, self)->IsDead());
}
static mrb_value rb_gactor_recover_all(mrb_state* mrb, mrb_value self) {
    GActorPtr(mrb, self)->RecoverAll();
    return mrb_nil_value();
}
static mrb_value GActorIntArray(mrb_state* mrb, const std::vector<int>& v) {
    mrb_value a = mrb_ary_new_capa(mrb, (mrb_int)v.size());
    for (int x : v) mrb_ary_push(mrb, a, mrb_int_value(mrb, x));
    return a;
}
static mrb_value rb_gactor_states(mrb_state* mrb, mrb_value self) {
    return GActorIntArray(mrb, GActorPtr(mrb, self)->states);
}
static mrb_value rb_gactor_add_state(mrb_state* mrb, mrb_value self) {
    mrb_int sid = 0;
    mrb_get_args(mrb, "i", &sid);
    auto& st = GActorPtr(mrb, self)->states;
    if (std::find(st.begin(), st.end(), (int)sid) == st.end()) st.push_back((int)sid);
    return GActorIntArray(mrb, st);
}
static mrb_value rb_gactor_remove_state(mrb_state* mrb, mrb_value self) {
    mrb_int sid = 0;
    mrb_get_args(mrb, "i", &sid);
    auto& st = GActorPtr(mrb, self)->states;
    st.erase(std::remove(st.begin(), st.end(), (int)sid), st.end());
    return GActorIntArray(mrb, st);
}
static mrb_value rb_gactor_skills(mrb_state* mrb, mrb_value self) {
    return GActorIntArray(mrb, GActorPtr(mrb, self)->skills);
}
static mrb_value rb_gactor_learn_skill(mrb_state* mrb, mrb_value self) {
    mrb_int sid = 0;
    mrb_get_args(mrb, "i", &sid);
    auto& sk = GActorPtr(mrb, self)->skills;
    if (std::find(sk.begin(), sk.end(), (int)sid) == sk.end()) sk.push_back((int)sid);
    return GActorIntArray(mrb, sk);
}
static mrb_value rb_gactor_forget_skill(mrb_state* mrb, mrb_value self) {
    mrb_int sid = 0;
    mrb_get_args(mrb, "i", &sid);
    auto& sk = GActorPtr(mrb, self)->skills;
    sk.erase(std::remove(sk.begin(), sk.end(), (int)sid), sk.end());
    return GActorIntArray(mrb, sk);
}
static mrb_value rb_gactor_weapon_id(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->weaponId);
}
static mrb_value GActorArmorSlot(mrb_state* mrb, mrb_value self, size_t slot) {
    const auto& ar = GActorPtr(mrb, self)->armors;
    return mrb_int_value(mrb, slot < ar.size() ? ar[slot] : 0);
}
static mrb_value rb_gactor_armor1_id(mrb_state* mrb, mrb_value self) { return GActorArmorSlot(mrb, self, 0); }
static mrb_value rb_gactor_armor2_id(mrb_state* mrb, mrb_value self) { return GActorArmorSlot(mrb, self, 1); }
static mrb_value rb_gactor_armor3_id(mrb_state* mrb, mrb_value self) { return GActorArmorSlot(mrb, self, 2); }
static mrb_value rb_gactor_armor4_id(mrb_state* mrb, mrb_value self) { return GActorArmorSlot(mrb, self, 3); }
static mrb_value rb_gactor_character_name(mrb_state* mrb, mrb_value self) {
    const auto& s = GActorPtr(mrb, self)->graphicName;
    return mrb_str_new(mrb, s.data(), (mrb_int)s.size());
}
static mrb_value rb_gactor_face_index(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GActorPtr(mrb, self)->faceIndex);
}

// XP Game_Actor#change_equip(equip_type, item) (Stufe 4g Teil 3): rein
// datengetrieben (KEIN Inventar-Tausch — das uebernimmt equip im Prelude,
// exakt wie beim Original). equip_type 0 = Waffe, 1..4 = Ruestungsslots;
// item = nil, Integer (ID) oder Objekt mit #id (RPG::Weapon/RPG::Armor).
static mrb_value rb_gactor_change_equip(mrb_state* mrb, mrb_value self) {
    mrb_int slot = 0;
    mrb_value item;
    mrb_get_args(mrb, "io", &slot, &item);
    int id = 0;
    if (mrb_integer_p(item) || mrb_float_p(item)) {
        id = (int)(mrb_integer_p(item) ? mrb_integer(item) : (mrb_int)mrb_float(item));
    } else if (!mrb_nil_p(item)) {
        mrb_value idv = mrb_funcall(mrb, item, "id", 0);
        if (mrb_integer_p(idv)) id = (int)mrb_integer(idv);
        if (mrb->exc) mrb->exc = nullptr; // item ohne #id -> wie nil werten
    }
    auto* a = GActorPtr(mrb, self);
    if (slot == 0) {
        a->weaponId = id;
    } else if (slot >= 1 && slot <= 4) {
        const size_t s = (size_t)(slot - 1);
        while (a->armors.size() <= s) a->armors.push_back(0);
        a->armors[s] = id;
    }
    return mrb_nil_value();
}

// ---------- XP Game_Enemy-Bruecke (Stufe 4g Teil 3) ----------
// Gespiegelt am Game_Actor-Muster: @battle_index >= 0 bindet die Instanz an
// den LIVE-Battler des laufenden Kampfs (BattleSystem::Enemies()); ohne
// Anbindung (lesende DB-Zugriffe, Aufbau vor Kampfbeginn) dient ein
// fluechtiger Orphan-Battler aus EnemyData als Quelle.
static std::unordered_map<int, Battler> s_orphanEnemies;
static Battler& EngineOrphanEnemyFor(int enemyId) {
    auto it = s_orphanEnemies.find(enemyId);
    if (it != s_orphanEnemies.end()) return it->second;
    Battler b;
    b.isActor = false;
    b.id = enemyId;
    b.index = -1;
    if (const auto* d = Database::Get().GetEnemy(enemyId)) {
        b.name = d->name;
        b.hp = b.maxHp = d->maxHp;
        b.mp = b.maxMp = d->maxMp;
        b.atk = d->atk; b.def = d->def; b.agi = d->agi;
    }
    auto res = s_orphanEnemies.emplace(enemyId, std::move(b));
    return res.first->second;
}
static mrb_int GEnemyBattleIndex(mrb_state* mrb, mrb_value self) {
    return mrb_as_int(mrb, mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@battle_index")));
}
static int GEnemyId(mrb_state* mrb, mrb_value self) {
    return (int)mrb_as_int(mrb, mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@enemy_id")));
}
static Battler* GEnemyLive(mrb_state* mrb, mrb_value self) {
    if (!BattleSystem::Get().IsInBattle()) return nullptr;
    const mrb_int idx = GEnemyBattleIndex(mrb, self);
    auto& list = BattleSystem::Get().Enemies();
    if (idx < 0 || idx >= (mrb_int)list.size()) return nullptr;
    return &list[(size_t)idx];
}
// Effektive Quelle: Live-Battler bevorzugt, sonst Orphan.
static Battler& GEnemyPtr(mrb_state* mrb, mrb_value self) {
    if (Battler* live = GEnemyLive(mrb, self)) return *live;
    return EngineOrphanEnemyFor(GEnemyId(mrb, self));
}
static const EnemyData* GEnemyData(mrb_state* mrb, mrb_value self) {
    if (Battler* live = GEnemyLive(mrb, self))
        return Database::Get().GetEnemy(live->id);
    return Database::Get().GetEnemy(GEnemyId(mrb, self));
}

static mrb_value rb_genemy_initialize(mrb_state* mrb, mrb_value self) {
    mrb_int id = 1;
    mrb_get_args(mrb, "|i", &id);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@enemy_id"), mrb_int_value(mrb, id));
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@battle_index"), mrb_int_value(mrb, -1));
    return self;
}
// Intern (nur Prelude/Game_Troop): Bindung an den Kampf-Battler-Slot.
static mrb_value rb_genemy_attach(mrb_state* mrb, mrb_value self) {
    mrb_int idx = -1;
    mrb_get_args(mrb, "i", &idx);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@battle_index"), mrb_int_value(mrb, idx));
    return mrb_int_value(mrb, idx);
}
static mrb_value rb_genemy_id(mrb_state* mrb, mrb_value self) {
    if (Battler* live = GEnemyLive(mrb, self)) return mrb_int_value(mrb, live->id);
    return mrb_int_value(mrb, GEnemyId(mrb, self));
}
static mrb_value rb_genemy_index(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyBattleIndex(mrb, self));
}
static mrb_value rb_genemy_exist(mrb_state* mrb, mrb_value self) {
    if (GEnemyLive(mrb, self)) return mrb_true_value();
    return mrb_bool_value(Database::Get().GetEnemy(GEnemyId(mrb, self)) != nullptr);
}
static mrb_value rb_genemy_name(mrb_state* mrb, mrb_value self) {
    const auto& n = GEnemyPtr(mrb, self).name;
    return mrb_str_new(mrb, n.data(), (mrb_int)n.size());
}
static mrb_value rb_genemy_battler_name(mrb_state* mrb, mrb_value self) {
    const auto* d = GEnemyData(mrb, self);
    return mrb_str_new_cstr(mrb, d ? d->battlerName.c_str() : "");
}
static mrb_value rb_genemy_battler_hue(mrb_state* mrb, mrb_value self) {
    const auto* d = GEnemyData(mrb, self);
    return mrb_int_value(mrb, d ? d->battlerHue : 0);
}
static mrb_value rb_genemy_hp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).hp);
}
static mrb_value rb_genemy_hp_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    Battler& b = GEnemyPtr(mrb, self);
    b.hp = std::clamp((int)v, 0, b.maxHp); // XP-Clamp
    b.isDead = (b.hp <= 0);
    if (b.hp > 0 && v > 0) b.isDead = false; // Wiederbeleben per hp= (XP)
    return mrb_int_value(mrb, b.hp);
}
static mrb_value rb_genemy_sp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).mp);
}
static mrb_value rb_genemy_sp_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    Battler& b = GEnemyPtr(mrb, self);
    b.mp = std::clamp((int)v, 0, b.maxMp);
    return mrb_int_value(mrb, b.mp);
}
static mrb_value rb_genemy_maxhp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).maxHp);
}
static mrb_value rb_genemy_maxsp(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).maxMp);
}
static mrb_value rb_genemy_atk(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).atk);
}
static mrb_value rb_genemy_def(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).def);
}
static mrb_value rb_genemy_agi(mrb_state* mrb, mrb_value self) {
    return mrb_int_value(mrb, GEnemyPtr(mrb, self).agi);
}
static mrb_value rb_genemy_dead(mrb_state* mrb, mrb_value self) {
    const Battler& b = GEnemyPtr(mrb, self);
    return mrb_bool_value(b.isDead || b.hp <= 0);
}
static mrb_value rb_genemy_recover_all(mrb_state* mrb, mrb_value self) {
    Battler& b = GEnemyPtr(mrb, self);
    b.hp = b.maxHp;
    b.mp = b.maxMp;
    b.isDead = false;
    // XP loescht hier auch alle Zustaende — die States der Game_Enemy-
    // Bruecke leben als Ruby-Ivar (siehe Prelude-Reopen).
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@states"), mrb_ary_new(mrb));
    return mrb_nil_value();
}
static mrb_value rb_genemy_exp(mrb_state* mrb, mrb_value self) {
    const auto* d = GEnemyData(mrb, self);
    return mrb_int_value(mrb, d ? d->exp : 0);
}
static mrb_value rb_genemy_gold(mrb_state* mrb, mrb_value self) {
    const auto* d = GEnemyData(mrb, self);
    return mrb_int_value(mrb, d ? d->gold : 0);
}
// XP Game_Enemy#transform(enemy_id): wie der Event-Befehl 336 (EnemyTransform)
// in BattleSystem::ApplyEventCommand — neue Art uebernimmt Namen/Werte voll.
static mrb_value rb_genemy_transform(mrb_state* mrb, mrb_value self) {
    mrb_int newId = 0;
    mrb_get_args(mrb, "i", &newId);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@enemy_id"), mrb_int_value(mrb, newId));
    Battler& b = GEnemyPtr(mrb, self); // nach dem Ivar-Wechsel: live ODER frischer Orphan
    b.id = (int)newId;
    if (const auto* d = Database::Get().GetEnemy((int)newId)) {
        b.name = d->name;
        b.maxHp = d->maxHp; b.hp = d->maxHp;
        b.maxMp = d->maxMp; b.mp = d->maxMp;
        b.atk = d->atk; b.def = d->def; b.agi = d->agi;
        b.isDead = false;
    }
    return mrb_nil_value();
}
// EnemyData fuehrt (anders als XP RPG::Enemy) keine Animations-IDs — ehrlich 0.
static mrb_value rb_genemy_animation_zero(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_int_value(mrb, 0);
}
// XP Arrow_*-Bruecke (PAKET 6/i Rundung): Bildschirmposition des Battlers
// auf dem RGSS-Canvas, projiziert ueber Game::worldToScreenHook mit der
// Laufzeitkamera (dieselbe Mechanik wie Map-Animationen an Weltpositionen).
// Ohne Hook/nicht im Kampf -> 0 (ehrlich statt Phantasieposition).
static bool GBattlerProject(const Battler* b, float& outX, float& outY) {
    if (!b || !Game::Get().worldToScreenHook) return false;
    return Game::Get().worldToScreenHook(b->position, outX, outY);
}
static mrb_value rb_genemy_x(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GEnemyLive(mrb, self), x, y);
    return mrb_int_value(mrb, (mrb_int)x);
}
static mrb_value rb_genemy_y(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GEnemyLive(mrb, self), x, y);
    return mrb_int_value(mrb, (mrb_int)y);
}
// Dasselbe fuer Akteure (XP Arrow_Actor): Party-Battler anhand der
// Actor-ID finden (eindeutig in der Party), dann projizieren.
static const Battler* GActorLiveBattler(mrb_state* mrb, mrb_value self) {
    if (!BattleSystem::Get().IsInBattle()) return nullptr;
    const int id = (int)GActorId(mrb, self);
    const auto& list = BattleSystem::Get().Actors();
    for (const auto& b : list)
        if (b.isActor && b.id == id) return &b;
    return nullptr;
}
static mrb_value rb_gactor_x(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GActorLiveBattler(mrb, self), x, y);
    return mrb_int_value(mrb, (mrb_int)x);
}
static mrb_value rb_gactor_y(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GActorLiveBattler(mrb, self), x, y);
    return mrb_int_value(mrb, (mrb_int)y);
}
// screen_x/screen_y (XP Game_Battler-Attribute): Getter liefert den per
// Setter abgelegten Wert, wenn gesetzt (XP laesst Spritesets die Anzeige-
// Position am Battler ablegen) — sonst die Engine-Projektion; beim
// Setter: einfaches Ablegen (kein Renderer-Zugriff, XP-getreu).
static mrb_value GBattlerScreenGet(mrb_state* mrb, mrb_value self,
                                   const char* ivar, float projVal) {
    mrb_value ov = mrb_iv_get(mrb, self, mrb_intern(mrb, ivar, (mrb_int)strlen(ivar)));
    if (mrb_int_p(ov) || mrb_float_p(ov)) return ov;
    return mrb_int_value(mrb, (mrb_int)projVal);
}
static mrb_value GBattlerScreenSet(mrb_state* mrb, mrb_value self, const char* ivar) {
    mrb_int v = 0;
    mrb_get_args(mrb, "i", &v);
    mrb_iv_set(mrb, self, mrb_intern(mrb, ivar, (mrb_int)strlen(ivar)),
               mrb_int_value(mrb, v));
    return mrb_int_value(mrb, v);
}
static mrb_value rb_genemy_screen_x_get(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GEnemyLive(mrb, self), x, y);
    return GBattlerScreenGet(mrb, self, "@screen_x", x);
}
static mrb_value rb_genemy_screen_x_set(mrb_state* mrb, mrb_value self) {
    return GBattlerScreenSet(mrb, self, "@screen_x");
}
static mrb_value rb_genemy_screen_y_get(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GEnemyLive(mrb, self), x, y);
    return GBattlerScreenGet(mrb, self, "@screen_y", y);
}
static mrb_value rb_genemy_screen_y_set(mrb_state* mrb, mrb_value self) {
    return GBattlerScreenSet(mrb, self, "@screen_y");
}
static mrb_value rb_gactor_screen_x_get(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GActorLiveBattler(mrb, self), x, y);
    return GBattlerScreenGet(mrb, self, "@screen_x", x);
}
static mrb_value rb_gactor_screen_x_set(mrb_state* mrb, mrb_value self) {
    return GBattlerScreenSet(mrb, self, "@screen_x");
}
static mrb_value rb_gactor_screen_y_get(mrb_state* mrb, mrb_value self) {
    float x = 0.0f, y = 0.0f;
    (void)GBattlerProject(GActorLiveBattler(mrb, self), x, y);
    return GBattlerScreenGet(mrb, self, "@screen_y", y);
}
static mrb_value rb_gactor_screen_y_set(mrb_state* mrb, mrb_value self) {
    return GBattlerScreenSet(mrb, self, "@screen_y");
}

// ---------- XP Game_Troop-Bruecke (Stufe 4g Teil 3): nur die ID-Bruecke nativ ----------
// setup/members baut das Prelude in Ruby (gleiches Muster wie Game_Party).
// __enemy_ids(troop_id): LIVE-Kampf-Battler haben Vorrang, sonst TroopData.
static mrb_value rb_gtroop_enemy_ids(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int troopId = 0;
    mrb_get_args(mrb, "i", &troopId);
    std::vector<int> ids;
    auto& bs = BattleSystem::Get();
    if (bs.IsInBattle()) {
        // Die laufenden Battler sind die Wahrheit (Reihenfolge = Slot-Index).
        for (const auto& b : bs.Enemies()) ids.push_back(b.id);
    } else if (const auto* tr = Database::Get().GetTroop((int)troopId)) {
        ids = tr->members;
    }
    mrb_value ary = mrb_ary_new_capa(mrb, (mrb_int)ids.size());
    for (int id : ids) mrb_ary_push(mrb, ary, mrb_int_value(mrb, id));
    return ary;
}

// ---------- XP Game_Screen-Bruecke (Stufe 4g Teil 3) ----------
// Direkt an EventSystem::ScreenEffects gekoppelt (derselbe Zustand, den die
// Event-Befehle 223/224/225 bedienen). XP uebergibt Dauer in FRAMES bei
// 40 fps -> Sekunden = frames / 40.0 (dokumentierte Umrechnung).
static float XpFramesToSecs(mrb_int frames) {
    const double s = (double)frames / 40.0;
    return s > 0.0 ? (float)s : 0.0f;
}
static mrb_value rb_gscreen_start_flash(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value color;
    mrb_int frames = 0;
    mrb_get_args(mrb, "oi", &color, &frames);
    float r = 255, g = 255, b = 255, a = 255;
    if (!mrb_nil_p(color)) {
        mrb_value idv = mrb_iv_get(mrb, color, mrb_intern_lit(mrb, "__rgss_color_id"));
        if (mrb_int_p(idv)) {
            if (auto* cs = RgssColorGet((int)mrb_as_int(mrb, idv))) {
                r = cs->r; g = cs->g; b = cs->b; a = cs->a;
            }
        }
    }
    auto& fx = GetScreenEffects();
    fx.flashColor = Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    fx.flashDuration = XpFramesToSecs(frames);
    fx.flashTimer = fx.flashDuration;
    return mrb_nil_value();
}
static mrb_value rb_gscreen_flash_color(mrb_state* mrb, mrb_value self) {
    (void)self;
    const auto& fx = GetScreenEffects();
    if (fx.flashTimer <= 0.0f) return mrb_nil_value(); // XP: nil ausserhalb
    struct RClass* ccls = mrb_class_get(mrb, "Color");
    mrb_value args[4] = {
        mrb_float_value(mrb, fx.flashColor.r * 255.0f),
        mrb_float_value(mrb, fx.flashColor.g * 255.0f),
        mrb_float_value(mrb, fx.flashColor.b * 255.0f),
        mrb_float_value(mrb, fx.flashColor.a * 255.0f) };
    return mrb_obj_new(mrb, ccls, 4, args);
}
static mrb_value rb_gscreen_start_tone_change(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value tone;
    mrb_int frames = 0;
    mrb_get_args(mrb, "oi", &tone, &frames);
    auto& fx = GetScreenEffects();
    if (!mrb_nil_p(tone)) {
        mrb_value idv = mrb_iv_get(mrb, tone, mrb_intern_lit(mrb, "__rgss_tone_id"));
        if (mrb_int_p(idv)) {
            if (auto* ts = RgssToneGet((int)mrb_as_int(mrb, idv))) {
                fx.toneTarget = Color(ts->r / 255.0f, ts->g / 255.0f,
                                      ts->b / 255.0f, ts->gray / 255.0f);
            }
        }
    }
    fx.toneElapsed = 0.0f;
    fx.toneDuration = XpFramesToSecs(frames);
    if (fx.toneDuration <= 0.0f) fx.toneCurrent = fx.toneTarget; // XP: d=0 -> sofort
    return mrb_nil_value();
}
static mrb_value rb_gscreen_tone(mrb_state* mrb, mrb_value self) {
    (void)self;
    const auto& fx = GetScreenEffects();
    struct RClass* tcls = mrb_class_get(mrb, "Tone");
    mrb_value args[4] = {
        mrb_float_value(mrb, fx.toneCurrent.r * 255.0f),
        mrb_float_value(mrb, fx.toneCurrent.g * 255.0f),
        mrb_float_value(mrb, fx.toneCurrent.b * 255.0f),
        mrb_float_value(mrb, fx.toneCurrent.a * 255.0f) };
    return mrb_obj_new(mrb, tcls, 4, args);
}
static mrb_value rb_gscreen_start_shake(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int power = 5, speed = 10, frames = 0;
    mrb_get_args(mrb, "iii", &power, &speed, &frames);
    auto& fx = GetScreenEffects();
    fx.shakePower = (int)power;
    fx.shakeSpeed = (int)speed;
    fx.shakeDuration = fx.shakeTimer = XpFramesToSecs(frames);
    return mrb_nil_value();
}
// XP Game_Screen#shake liefert den aktuellen Versatz fuer den Viewport; der
// Renderer wendet ihn nativ an — hier als Integer-Naeherung (Kraft, solange
// der Timer laeuft, sonst 0), dokumentiert.
static mrb_value rb_gscreen_shake(mrb_state* mrb, mrb_value self) {
    (void)self;
    const auto& fx = GetScreenEffects();
    return mrb_int_value(mrb, fx.shakeTimer > 0.0f ? fx.shakePower : 0);
}
static mrb_value rb_gscreen_initialize(mrb_state* mrb, mrb_value self) {
    (void)mrb;
    return self;
}


// ---------- Menue/Speicherbildschirm aus Ruby oeffnen (XP: Scene_Menu/Scene_Save) ----------
static mrb_value rb_ui_open_menu(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    GameUI::Get().Pause().Show(); // XP-Spielmenue
    return mrb_nil_value();
}
static mrb_value rb_ui_open_save_screen(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_bool saveMode = 1;
    mrb_get_args(mrb, "|b", &saveMode);
    GameUI::Get().ShowSaveScreen(saveMode != 0);
    return mrb_nil_value();
}
// Bequemlichkeits-Alias: UI.open_load_screen = UI.open_save_screen(false)
static mrb_value rb_ui_open_load_screen(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    GameUI::Get().ShowSaveScreen(false);
    return mrb_nil_value();
}

// ---------- Eigene Menues aus Ruby (XP: eigene Scenes in Minuten) ----------
// UI.open_list_menu("Titel", ["Eins", ["Zwei", false], ...]) { |index| ... }
// Der Block bekommt den gewaehlten Index, bei Esc -1. Eintraege koennen als
// [text, enabled]-Paar einzeln gesperrt werden.
static mrb_value rb_ui_open_list_menu(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value title, entries, blk;
    mrb_get_args(mrb, "oo&", &title, &entries, &blk);

    std::string titleStr;
    if (mrb_string_p(title)) titleStr.assign(RSTRING_PTR(title), (size_t)RSTRING_LEN(title));

    std::vector<MenuWindow::Entry> items;
    if (mrb_array_p(entries)) {
        const mrb_int n = RARRAY_LEN(entries);
        for (mrb_int i = 0; i < n; ++i) {
            mrb_value e = mrb_ary_ref(mrb, entries, i);
            if (mrb_string_p(e)) {
                items.push_back({std::string(RSTRING_PTR(e), (size_t)RSTRING_LEN(e)), true});
            } else if (mrb_array_p(e)) { // [text, enabled]
                mrb_value t = mrb_ary_ref(mrb, e, 0);
                mrb_value en = mrb_ary_ref(mrb, e, 1);
                std::string ts;
                if (mrb_string_p(t)) ts.assign(RSTRING_PTR(t), (size_t)RSTRING_LEN(t));
                items.push_back({ts, mrb_nil_p(en) || mrb_test(en)});
            }
        }
    }
    if (items.empty()) items.push_back({"(leer)", false});

    // Block global parken (GC-sicher); der C++-Rueckruf holt ihn sich.
    // (Kein Block = nil -> CallListMenuBlock tut dann nichts.)
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$__rpg3d_menu_block"), blk);
    Engine* e = static_cast<Engine*>(mrb->ud);
    GameUI::Get().Menu().Show(titleStr, items, [e](int idx) {
        if (e) e->GetRubyVM().CallListMenuBlock(idx);
    }, true);
    GameUI::Get().Menu().onCancel = [e]() {
        if (e) e->GetRubyVM().CallListMenuBlock(-1);
    };
    return mrb_nil_value();
}

// ---------- Namenseingabe aus Ruby (XP Befehl 303 auch fuer Custom-Szenen) ----------
// UI.open_name_input(actor_id = nil, max_chars = 8, prompt = "") { |name| ... }
// Mit actor_id (Integer) verhaelt es sich exakt wie der Event-Befehl 303:
// Starttext = aktueller Actor-Name, Ergebnis wird zurueckgeschrieben UND
// zusaetzlich an den Block gegeben. actor_id = nil: reine Eingabe, nur Block.
// Der Block wird GC-sicher global geparkt (Muster wie UI.open_list_menu).
static mrb_value rb_ui_open_name_input(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_value actorId;
    mrb_int maxChars = 8;
    char* prompt = nullptr;
    mrb_value blk;
    mrb_get_args(mrb, "o|iz&", &actorId, &maxChars, &prompt, &blk);

    int aid = 0;
    std::string initial;
    if (mrb_integer_p(actorId)) {
        aid = (int)mrb_integer(actorId);
        if (auto* a = Game::Get().Party().GetActor(aid)) initial = a->name;
    }
    mrb_gv_set(mrb, mrb_intern_lit(mrb, "$__rpg3d_nameinput_block"), blk);
    Engine* e = static_cast<Engine*>(mrb->ud);
    GameUI::Get().ShowNameInput(prompt ? prompt : "", initial,
        (int)(maxChars > 0 ? maxChars : 8),
        [e, aid](const std::string& name) {
            if (aid > 0) {
                if (auto* a = Game::Get().Party().GetActor(aid)) a->name = name;
            }
            if (e) e->GetRubyVM().CallNameInputResult(name);
        });
    return mrb_nil_value();
}

// ---------- Native-UI-Schalter aus Ruby (Pendant zu Game.ini) ----------
#define DEF_NATIVE_FLAG(Name, Field) \
    static mrb_value rb_ui_native_##Name##_set(mrb_state* mrb, mrb_value self) { \
        (void)self; mrb_bool v; mrb_get_args(mrb, "b", &v); \
        CustomConfig::Get().Field = (v != 0); \
        return mrb_bool_value(v); } \
    static mrb_value rb_ui_native_##Name##_get(mrb_state* mrb, mrb_value self) { \
        (void)mrb; (void)self; \
        return mrb_bool_value(CustomConfig::Get().Field); }
DEF_NATIVE_FLAG(title, nativeTitle)
DEF_NATIVE_FLAG(hud, nativeHud)
DEF_NATIVE_FLAG(game_menu, nativeGameMenu)
DEF_NATIVE_FLAG(battle_menu, nativeBattleMenu)
DEF_NATIVE_FLAG(battle_status, nativeBattleStatus)
DEF_NATIVE_FLAG(xp_scene_mode, xpSceneMode) // PAKET 6/h: XP-Szenen-Framework
#undef DEF_NATIVE_FLAG
// native_hud= schaltet zusaetzlich LIVE die HUD-Sichtbarkeit um
// (PAKET 10: GameUI-ImGui-HUD statt RmlUi)
static mrb_value rb_ui_native_hud_set_live(mrb_state* mrb, mrb_value self) {
    mrb_value v = rb_ui_native_hud_set(mrb, self);
    GameUI::Get().SetHudVisible(CustomConfig::Get().nativeHud);
    return v;
}

// ---------- Game.new_game / Game.start_game (fuer Custom-Titel) ----------
static mrb_value rb_game_new_game(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    Game::Get().NewGame();
    return mrb_nil_value();
}
// Komplettstart aus einem Custom-Titel: NewGame + Spielmodus an
static mrb_value rb_game_start_game(mrb_state* mrb, mrb_value self) {
    (void)self;
    Game::Get().NewGame();
    Engine* e = static_cast<Engine*>(mrb->ud);
    if (e) e->SetPlaying(true);
    return mrb_nil_value();
}

// ---------- HUD an/aus aus Ruby (UI.hud_visible = true/false) ----------
// PAKET 10: steuert das GameUI-ImGui-HUD (RmlUi ist entfernt)
static mrb_value rb_ui_hud_set_visible(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_bool v;
    mrb_get_args(mrb, "b", &v);
    GameUI::Get().SetHudVisible(v);
    return mrb_bool_value(v);
}
static mrb_value rb_ui_hud_visible(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_bool_value(GameUI::Get().IsHudVisible());
}

// ---------------------------------------------------------------------------
// RGSS: Ruby-Klasse "Window" (reine Ruby-UI, unabhaengig von RmlUi)
// ---------------------------------------------------------------------------
static rpg::RgssWindowState* RgssWinFrom(mrb_state* mrb, mrb_value self) {
    mrb_value idv = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "__rgss_id"));
    if (!mrb_integer_p(idv)) return nullptr;
    return rpg::RgssUI::Get().GetWindow((int)mrb_integer(idv));
}

static mrb_value rb_win_init(mrb_state* mrb, mrb_value self) {
    mrb_float x = 0, y = 0, w = 0, h = 0;
    mrb_get_args(mrb, "|ffff", &x, &y, &w, &h);
    const int id = rpg::RgssUI::Get().MakeWindow((float)x, (float)y, (float)w, (float)h);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_id"), RPG_MRB_INT_VALUE(mrb, id));
    return self;
}

#define RGSS_WIN_FATTR(rname, field)                                              \
static mrb_value rb_win_##rname##_get(mrb_state* mrb, mrb_value self) {          \
    if (auto* w = RgssWinFrom(mrb, self)) return mrb_float_value(mrb, w->field); \
    return mrb_nil_value();                                                       \
}                                                                                 \
static mrb_value rb_win_##rname##_set(mrb_state* mrb, mrb_value self) {          \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* w = RgssWinFrom(mrb, self)) w->field = (float)v;                    \
    return mrb_float_value(mrb, v);                                               \
}
RGSS_WIN_FATTR(x, x)
RGSS_WIN_FATTR(y, y)
RGSS_WIN_FATTR(width, width)
RGSS_WIN_FATTR(height, height)
RGSS_WIN_FATTR(openness, openness)

static mrb_value rb_win_z_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, w->z);
    return mrb_nil_value();
}
static mrb_value rb_win_z_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* w = RgssWinFrom(mrb, self)) w->z = (int)v;
    return RPG_MRB_INT_VALUE(mrb, v);
}

static mrb_value rb_win_visible_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom(mrb, self)) return mrb_bool_value(w->visible);
    return mrb_bool_value(false);
}
static mrb_value rb_win_visible_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    if (auto* w = RgssWinFrom(mrb, self)) w->visible = v;
    return mrb_bool_value(v);
}

static mrb_value rb_win_skin_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom(mrb, self)) return mrb_str_new_cstr(mrb, w->windowskin.c_str());
    return mrb_nil_value();
}
static mrb_value rb_win_skin_set(mrb_state* mrb, mrb_value self) {
    mrb_value v; mrb_get_args(mrb, "o", &v);
    if (auto* w = RgssWinFrom(mrb, self)) {
        if (mrb_string_p(v)) w->windowskin.assign(RSTRING_PTR(v), (size_t)RSTRING_LEN(v));
        else w->windowskin.clear();
    }
    return v;
}

static mrb_value rb_win_text_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom(mrb, self)) return mrb_str_new_cstr(mrb, w->text.c_str());
    return mrb_nil_value();
}
static mrb_value rb_win_text_set(mrb_state* mrb, mrb_value self) {
    mrb_value v; mrb_get_args(mrb, "o", &v);
    if (auto* w = RgssWinFrom(mrb, self)) {
        if (mrb_string_p(v)) w->text.assign(RSTRING_PTR(v), (size_t)RSTRING_LEN(v));
        else w->text.clear();
    }
    return v;
}

// text_color = [r, g, b, a]  (0.0 .. 1.0, Array; a optional = 1.0)
static mrb_value rb_win_text_color_set(mrb_state* mrb, mrb_value self) {
    mrb_value v; mrb_get_args(mrb, "o", &v);
    auto* w = RgssWinFrom(mrb, self);
    if (w && mrb_array_p(v)) {
        const mrb_int len = RARRAY_LEN(v);
        const auto fget = [&](int idx, float fallback) {
            if (idx < len) {
                mrb_value e = mrb_ary_ref(mrb, v, idx);
                if (mrb_float_p(e)) return (float)mrb_float(e);
                if (mrb_integer_p(e)) return (float)mrb_integer(e);
            }
            return fallback;
        };
        w->textRed = fget(0, 1.0f);
        w->textGreen = fget(1, 1.0f);
        w->textBlue = fget(2, 1.0f);
        w->textAlpha = fget(3, 1.0f);
    }
    return v;
}

static mrb_value rb_win_dispose(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom(mrb, self)) w->disposed = true;
    return mrb_nil_value();
}
static mrb_value rb_win_disposed_p(mrb_state* mrb, mrb_value self) {
    const auto* w = RgssWinFrom(mrb, self);
    return mrb_bool_value(!w || w->disposed);
}
static mrb_value rb_win_update(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self; // RGSS-Kompatibilitaets-Hook (no-op)
    return mrb_nil_value();
}

static mrb_value rb_rgss_clear_windows(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    rpg::RgssUI::Get().ClearAll();
    return mrb_nil_value();
}

// ===========================================================================
// PAKET 32: Rui — eigenes UI-Framework, Ruby-Bindings (Script-Windows)
//
// Handle-Modell: Die Ruby-Objekte (Rui::Window/Label/Gauge/ListView) tragen
// nur IDs (@__wid Fenster, @__cid Widget). Die Objekte leben in
// rui::Manager; verschwindet ein Fenster (Close/destroy/clear), werden
// spaetere Zugriffe zu sicheren No-Ops (kein dangling native pointer).
// Bloecke (on_pick/on_cancel) werden unter "$__rpg3d_rui_blocks"[key][kind]
// GC-sicher geparkt; Listen-Lambdas rufen RubyVM::CallRuiBlock.
// ===========================================================================
namespace {

constexpr const char* kRuiBlocksVar = "$__rpg3d_rui_blocks";

struct RuiHandleMaps {
    struct RClass* mod = nullptr;
    struct RClass* win = nullptr;
    struct RClass* label = nullptr;
    struct RClass* gauge = nullptr;
    struct RClass* list = nullptr;
};
RuiHandleMaps g_rui;

std::string RuiIvarStr(mrb_state* mrb, mrb_value self, const char* name) {
    mrb_value v = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, name));
    if (!mrb_string_p(v)) return {};
    return std::string(RSTRING_PTR(v), (size_t)RSTRING_LEN(v));
}

mrb_value RuiMakeHandle(mrb_state* mrb, struct RClass* klass,
                        const std::string& wid, const std::string& cid) {
    mrb_value obj = mrb_obj_new(mrb, klass, 0, nullptr);
    mrb_iv_set(mrb, obj, mrb_intern_lit(mrb, "@__wid"),
               mrb_str_new(mrb, wid.data(), (mrb_int)wid.size()));
    mrb_iv_set(mrb, obj, mrb_intern_lit(mrb, "@__cid"),
               mrb_str_new(mrb, cid.data(), (mrb_int)cid.size()));
    return obj;
}

rui::Window* RuiResolveWin(mrb_state* mrb, mrb_value self) {
    const std::string wid = RuiIvarStr(mrb, self, "@__wid");
    if (wid.empty()) return nullptr;
    return rui::Manager::Get().FindWindow(wid);
}

// Kind-Widget beliebigen Typs im Fenster aufloesen (cid leer = Fenster selbst)
rui::Widget* RuiResolveWidget(mrb_state* mrb, mrb_value self) {
    rui::Window* win = RuiResolveWin(mrb, self);
    if (!win) return nullptr;
    const std::string cid = RuiIvarStr(mrb, self, "@__cid");
    if (cid.empty()) return win;
    return win->FindWidget(cid);
}

// Haengt Pick/Hover/Cancel-Lambdas einer Liste an den Ruby-Block-Store-
// Aufrufpfad (key = "<wid>/<cid>", kind in {"pick","hover","cancel"}).
void RuiWireListCallbacks(mrb_state* mrb, rui::ListView* lv, const std::string& key) {
    Engine* eng = static_cast<Engine*>(mrb->ud);
    lv->onPick = [eng, key](int idx) {
        if (eng) eng->GetRubyVM().CallRuiBlock(key.c_str(), idx, "pick");
    };
    lv->onHoverItem = [eng, key](int idx) {
        if (eng) eng->GetRubyVM().CallRuiBlock(key.c_str(), idx, "hover");
    };
    lv->onCancel = [eng, key]() {
        if (eng) eng->GetRubyVM().CallRuiBlock(key.c_str(), -1, "cancel");
    };
}

void RuiStoreBlock(mrb_state* mrb, const std::string& key, const char* kind, mrb_value blk) {
    mrb_value all = mrb_gv_get(mrb, mrb_intern_lit(mrb, kRuiBlocksVar));
    if (!mrb_hash_p(all)) {
        all = mrb_hash_new(mrb);
        mrb_gv_set(mrb, mrb_intern_lit(mrb, kRuiBlocksVar), all);
    }
    mrb_value k = mrb_str_new(mrb, key.data(), (mrb_int)key.size());
    mrb_value entry = mrb_hash_get(mrb, all, k);
    if (!mrb_hash_p(entry)) {
        entry = mrb_hash_new(mrb);
        mrb_hash_set(mrb, all, k, entry);
    }
    mrb_hash_set(mrb, entry, mrb_symbol_value(mrb_intern_lit(mrb, kind)), blk);
}

// ---------- Modul-Funktionen Rui.* ----------
mrb_value rb_rui_window_create(mrb_state* mrb, mrb_value) {
    char* id = nullptr; mrb_float x = 0, y = 0, w = 100, h = 100;
    mrb_get_args(mrb, "zffff", &id, &x, &y, &w, &h);
    if (!id || !*id) return mrb_nil_value();
    if (rui::Manager::Get().FindWindow(id))
        rui::Manager::Get().RemoveWindow(id); // Neuaufbau ersetzt gleichnamiges Fenster
    auto nw = std::make_unique<rui::Window>();
    nw->id = id;
    nw->rect = rui::Rect{(float)x, (float)y, (float)w, (float)h};
    rui::Window& ref = rui::Manager::Get().AddWindow(std::move(nw));
    (void)ref;
    return RuiMakeHandle(mrb, g_rui.win, id, "");
}

mrb_value rb_rui_window_find(mrb_state* mrb, mrb_value) {
    char* id = nullptr;
    mrb_get_args(mrb, "z", &id);
    if (!id || !rui::Manager::Get().FindWindow(id)) return mrb_nil_value();
    return RuiMakeHandle(mrb, g_rui.win, id, "");
}

mrb_value rb_rui_destroy(mrb_state*, mrb_value) {
    // rein: entfernt sofort (fuer Animation: win.close)
    return mrb_nil_value(); // wird auf self-Methode abgebildet, s.u.
}

mrb_value rb_rui_clear(mrb_state* mrb, mrb_value) {
    (void)mrb;
    rui::Manager::Get().Clear();
    return mrb_nil_value();
}

mrb_value rb_rui_set_focus_list(mrb_state* mrb, mrb_value) {
    char* wid = nullptr; char* cid = nullptr;
    mrb_get_args(mrb, "zz", &wid, &cid);
    if (wid && cid) rui::Manager::Get().SetFocusList(std::string(wid) + "/" + cid);
    return mrb_nil_value();
}

mrb_value rb_rui_clear_focus(mrb_state* mrb, mrb_value) {
    (void)mrb;
    rui::Manager::Get().ClearFocus();
    return mrb_nil_value();
}

mrb_value rb_rui_has_focus(mrb_state* mrb, mrb_value) {
    (void)mrb;
    return mrb_bool_value(rui::Manager::Get().HasFocus());
}

// PAKET 33: Rui.windowskin = "name" (ohne Endung) — aufgeloest ueber die
// XP-Projektstruktur (Graphics/System/... zuerst). "" oder nil = Flat-Skin.
mrb_value rb_rui_windowskin_set(mrb_state* mrb, mrb_value) {
    mrb_value name = mrb_nil_value();
    mrb_get_args(mrb, "o", &name);
    if (mrb_nil_p(name) || (mrb_string_p(name) && RSTRING_LEN(name) == 0)) {
        rui::Manager::Get().ClearSkin();
        return mrb_nil_value();
    }
    if (!mrb_string_p(name)) return mrb_nil_value();
    Engine* e = static_cast<Engine*>(mrb->ud);
    std::string nm(RSTRING_PTR(name), (size_t)RSTRING_LEN(name));
    std::string path = e ? e->ResolvePicturePathFor(nm) : std::string();
    if (path.empty() && e) {
        // haertester Fallbacks: explizit Graphics/System suchen
        path = e->ResolvePicturePathFor(nm);
    }
    if (!path.empty()) rui::Manager::Get().SetSkinSource(path);
    return mrb_nil_value();
}

mrb_value rb_rui_windowskin_get(mrb_state* mrb, mrb_value) {
    (void)mrb;
    const std::string& s = rui::Manager::Get().GetSkinSource();
    return mrb_str_new(mrb, s.data(), (mrb_int)s.size());
}

// ---------- Rui::Window-Methoden ----------
mrb_value rb_rui_win_id(mrb_state* mrb, mrb_value self) {
    const std::string s = RuiIvarStr(mrb, self, "@__wid");
    return mrb_str_new(mrb, s.data(), (mrb_int)s.size());
}

mrb_value rb_rui_win_move(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, w, h;
    mrb_get_args(mrb, "ffff", &x, &y, &w, &h);
    if (auto* win = RuiResolveWin(mrb, self))
        win->rect = rui::Rect{(float)x, (float)y, (float)w, (float)h};
    return mrb_nil_value();
}

mrb_value rb_rui_win_x(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_float_value(mrb, win->rect.x);
    return mrb_float_value(mrb, 0);
}
mrb_value rb_rui_win_y(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_float_value(mrb, win->rect.y);
    return mrb_float_value(mrb, 0);
}
mrb_value rb_rui_win_w(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_float_value(mrb, win->rect.w);
    return mrb_float_value(mrb, 0);
}
mrb_value rb_rui_win_h(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_float_value(mrb, win->rect.h);
    return mrb_float_value(mrb, 0);
}
mrb_value rb_rui_win_x_set(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);
    if (auto* win = RuiResolveWin(mrb, self)) win->rect.x = (float)v;
    return mrb_nil_value();
}
mrb_value rb_rui_win_y_set(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);
    if (auto* win = RuiResolveWin(mrb, self)) win->rect.y = (float)v;
    return mrb_nil_value();
}
mrb_value rb_rui_win_w_set(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);
    if (auto* win = RuiResolveWin(mrb, self)) win->rect.w = (float)v;
    return mrb_nil_value();
}
mrb_value rb_rui_win_h_set(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);
    if (auto* win = RuiResolveWin(mrb, self)) win->rect.h = (float)v;
    return mrb_nil_value();
}

mrb_value rb_rui_win_openness(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_float_value(mrb, win->openness);
    return mrb_float_value(mrb, 0);
}
mrb_value rb_rui_win_openness_set(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);
    if (auto* win = RuiResolveWin(mrb, self))
        win->openness = std::clamp((float)v, 0.0f, 255.0f);
    return mrb_nil_value();
}

mrb_value rb_rui_win_open(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) { win->SetClosing(false); win->Open(); }
    return mrb_nil_value();
}
mrb_value rb_rui_win_close(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) win->SetClosing(true);
    return mrb_nil_value();
}
mrb_value rb_rui_win_closing_p(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_bool_value(win->IsClosing());
    return mrb_bool_value(false);
}
mrb_value rb_rui_win_open_p(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_bool_value(win->IsFullyOpen());
    return mrb_bool_value(false);
}

mrb_value rb_rui_win_visible(mrb_state* mrb, mrb_value self) {
    if (auto* win = RuiResolveWin(mrb, self)) return mrb_bool_value(win->visible);
    return mrb_bool_value(false);
}
mrb_value rb_rui_win_visible_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = MRUBY_TRUE; mrb_get_args(mrb, "b", &v);
    if (auto* win = RuiResolveWin(mrb, self)) win->visible = !!v;
    return mrb_nil_value();
}

mrb_value rb_rui_win_add_label(mrb_state* mrb, mrb_value self) {
    char* cid = nullptr; char* text = nullptr;
    mrb_float x = 0, y = 0, align = 0, scale = 1.0;
    mrb_get_args(mrb, "zz|ffff", &cid, &text, &x, &y, &align, &scale);
    rui::Window* win = RuiResolveWin(mrb, self);
    if (!win || !cid || !*cid) return mrb_nil_value();
    auto lbl = std::make_unique<rui::Label>();
    lbl->id = cid;
    lbl->text = text ? text : "";
    lbl->align = std::clamp((int)align, 0, 2);
    lbl->scale = (float)scale;
    const auto& th = rui::Theme::Get();
    lbl->rect = rui::Rect{win->rect.x + th.padding + (float)x,
                          win->rect.y + th.padding + (float)y,
                          win->rect.w - 2.0f * th.padding - (float)x,
                          th.rowHeight * (float)scale};
    lbl->color = th.text;
    win->children.push_back(std::move(lbl));
    return RuiMakeHandle(mrb, g_rui.label, win->id, cid);
}

mrb_value rb_rui_win_add_gauge(mrb_state* mrb, mrb_value self) {
    // add_gauge(id, x, y, w, h, current=0, maximum=100, kind="hp")
    // kind: "hp" (gruen) / "mp" (blau) — Farben aus dem Theme, spaeter
    // per set_color uebersteuerbar.
    char* cid = nullptr; char* kind = nullptr;
    mrb_float x = 0, y = 0, w = 100, h = 10;
    mrb_int cur = 0, mx2 = 100;
    mrb_get_args(mrb, "zffff|iiz", &cid, &x, &y, &w, &h, &cur, &mx2, &kind);
    rui::Window* win = RuiResolveWin(mrb, self);
    if (!win || !cid || !*cid) return mrb_nil_value();
    auto g = std::make_unique<rui::Gauge>();
    g->id = cid;
    g->current = (int)cur;
    g->maximum = (int)mx2;
    const auto& th = rui::Theme::Get();
    g->color = (kind && std::string(kind) == "mp") ? th.gaugeMp : th.gaugeHp;
    g->rect = rui::Rect{win->rect.x + th.padding + (float)x,
                        win->rect.y + th.padding + (float)y,
                        (float)w, (float)h};
    win->children.push_back(std::move(g));
    return RuiMakeHandle(mrb, g_rui.gauge, win->id, cid);
}

mrb_value rb_rui_win_add_list(mrb_state* mrb, mrb_value self) {
    char* cid = nullptr; mrb_value items; mrb_float x = 0, y = 0, w = 100, h = 100;
    mrb_get_args(mrb, "zo|ffff", &cid, &items, &x, &y, &w, &h);
    rui::Window* win = RuiResolveWin(mrb, self);
    if (!win || !cid || !*cid) return mrb_nil_value();
    auto lv = std::make_unique<rui::ListView>();
    lv->id = cid;
    if (mrb_array_p(items)) {
        const mrb_int n = RARRAY_LEN(items);
        for (mrb_int i = 0; i < n; ++i) {
            mrb_value e = mrb_ary_ref(mrb, items, i);
            if (mrb_string_p(e)) {
                lv->items.push_back({std::string(RSTRING_PTR(e), (size_t)RSTRING_LEN(e)), true});
            } else if (mrb_array_p(e)) { // [text, enabled]
                mrb_value t = mrb_ary_ref(mrb, e, 0);
                mrb_value en = mrb_ary_ref(mrb, e, 1);
                std::string ts;
                if (mrb_string_p(t)) ts.assign(RSTRING_PTR(t), (size_t)RSTRING_LEN(t));
                lv->items.push_back({ts, mrb_nil_p(en) || mrb_test(en)});
            }
        }
    }
    const auto& th = rui::Theme::Get();
    lv->rect = rui::Rect{win->rect.x + th.padding + (float)x,
                         win->rect.y + th.padding + (float)y, (float)w, (float)h};
    lv->selected = lv->items.empty() ? -1 : 0;
    const std::string key = win->id + "/" + cid;
    RuiWireListCallbacks(mrb, lv.get(), key);
    win->children.push_back(std::move(lv));
    return RuiMakeHandle(mrb, g_rui.list, win->id, cid);
}

mrb_value rb_rui_win_remove_widget(mrb_state* mrb, mrb_value self) {
    char* cid = nullptr; mrb_get_args(mrb, "z", &cid);
    rui::Window* win = RuiResolveWin(mrb, self);
    if (!win || !cid) return mrb_nil_value();
    win->children.erase(std::remove_if(win->children.begin(), win->children.end(),
        [&](const std::unique_ptr<rui::Widget>& c) { return c->id == cid; }),
        win->children.end());
    return mrb_nil_value();
}

mrb_value rb_rui_win_destroy(mrb_state* mrb, mrb_value self) {
    const std::string wid = RuiIvarStr(mrb, self, "@__wid");
    if (!wid.empty()) rui::Manager::Get().RemoveWindow(wid);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@__wid"), mrb_str_new(mrb, "", 0));
    return mrb_nil_value();
}

// ---------- Rui::Label-Methoden ----------
mrb_value rb_rui_label_text(mrb_state* mrb, mrb_value self) {
    if (auto* w = dynamic_cast<rui::Label*>(RuiResolveWidget(mrb, self)))
        return mrb_str_new(mrb, w->text.data(), (mrb_int)w->text.size());
    return mrb_str_new(mrb, nullptr, 0);
}
mrb_value rb_rui_label_text_set(mrb_state* mrb, mrb_value self) {
    char* t = nullptr; mrb_get_args(mrb, "z", &t);
    if (auto* w = dynamic_cast<rui::Label*>(RuiResolveWidget(mrb, self)))
        w->text = t ? t : "";
    return mrb_nil_value();
}
mrb_value rb_rui_label_color_set(mrb_state* mrb, mrb_value self) {
    mrb_float r = 1, g = 1, b = 1, a = 1;
    mrb_get_args(mrb, "f|fff", &r, &g, &b, &a);
    if (auto* w = dynamic_cast<rui::Label*>(RuiResolveWidget(mrb, self)))
        w->color = rui::Color4((float)r, (float)g, (float)b, (float)a);
    return mrb_nil_value();
}

// ---------- Rui::Gauge-Methoden ----------
mrb_value rb_rui_gauge_current(mrb_state* mrb, mrb_value self) {
    if (auto* w = dynamic_cast<rui::Gauge*>(RuiResolveWidget(mrb, self)))
        return mrb_int_value(mrb, w->current);
    return mrb_int_value(mrb, 0);
}
mrb_value rb_rui_gauge_current_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* w = dynamic_cast<rui::Gauge*>(RuiResolveWidget(mrb, self)))
        w->current = (int)v;
    return mrb_nil_value();
}
mrb_value rb_rui_gauge_maximum(mrb_state* mrb, mrb_value self) {
    if (auto* w = dynamic_cast<rui::Gauge*>(RuiResolveWidget(mrb, self)))
        return mrb_int_value(mrb, w->maximum);
    return mrb_int_value(mrb, 0);
}
mrb_value rb_rui_gauge_maximum_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* w = dynamic_cast<rui::Gauge*>(RuiResolveWidget(mrb, self)))
        w->maximum = (int)v;
    return mrb_nil_value();
}
mrb_value rb_rui_gauge_color_set(mrb_state* mrb, mrb_value self) {
    mrb_float r = 1, g = 1, b = 1, a = 1;
    mrb_get_args(mrb, "f|fff", &r, &g, &b, &a);
    if (auto* w = dynamic_cast<rui::Gauge*>(RuiResolveWidget(mrb, self)))
        w->color = rui::Color4((float)r, (float)g, (float)b, (float)a);
    return mrb_nil_value();
}

// ---------- Rui::ListView-Methoden ----------
mrb_value rb_rui_list_selected(mrb_state* mrb, mrb_value self) {
    if (auto* w = dynamic_cast<rui::ListView*>(RuiResolveWidget(mrb, self)))
        return mrb_int_value(mrb, w->selected);
    return mrb_int_value(mrb, -1);
}
mrb_value rb_rui_list_selected_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* w = dynamic_cast<rui::ListView*>(RuiResolveWidget(mrb, self)))
        if (v >= 0 && v < (mrb_int)w->items.size()) w->selected = (int)v;
    return mrb_nil_value();
}
mrb_value rb_rui_list_items_set(mrb_state* mrb, mrb_value self) {
    mrb_value items; mrb_get_args(mrb, "o", &items);
    auto* w = dynamic_cast<rui::ListView*>(RuiResolveWidget(mrb, self));
    if (!w) return mrb_nil_value();
    w->items.clear();
    if (mrb_array_p(items)) {
        const mrb_int n = RARRAY_LEN(items);
        for (mrb_int i = 0; i < n; ++i) {
            mrb_value e = mrb_ary_ref(mrb, items, i);
            if (mrb_string_p(e))
                w->items.push_back({std::string(RSTRING_PTR(e), (size_t)RSTRING_LEN(e)), true});
            else if (mrb_array_p(e)) {
                mrb_value t = mrb_ary_ref(mrb, e, 0);
                mrb_value en = mrb_ary_ref(mrb, e, 1);
                std::string ts;
                if (mrb_string_p(t)) ts.assign(RSTRING_PTR(t), (size_t)RSTRING_LEN(t));
                w->items.push_back({ts, mrb_nil_p(en) || mrb_test(en)});
            }
        }
    }
    w->selected = w->items.empty() ? -1 : std::clamp(w->selected, 0, (int)w->items.size() - 1);
    return mrb_nil_value();
}
mrb_value rb_rui_list_on_pick(mrb_state* mrb, mrb_value self) {
    mrb_value blk; mrb_get_args(mrb, "&", &blk);
    const std::string key = RuiIvarStr(mrb, self, "@__wid") + "/" + RuiIvarStr(mrb, self, "@__cid");
    RuiStoreBlock(mrb, key, "pick", blk);
    return mrb_nil_value();
}
mrb_value rb_rui_list_on_cancel(mrb_state* mrb, mrb_value self) {
    mrb_value blk; mrb_get_args(mrb, "&", &blk);
    const std::string key = RuiIvarStr(mrb, self, "@__wid") + "/" + RuiIvarStr(mrb, self, "@__cid");
    RuiStoreBlock(mrb, key, "cancel", blk);
    return mrb_nil_value();
}
mrb_value rb_rui_list_on_hover(mrb_state* mrb, mrb_value self) {
    mrb_value blk; mrb_get_args(mrb, "&", &blk);
    const std::string key = RuiIvarStr(mrb, self, "@__wid") + "/" + RuiIvarStr(mrb, self, "@__cid");
    RuiStoreBlock(mrb, key, "hover", blk);
    return mrb_nil_value();
}

} // namespace

void RubyVM::CallRuiBlock(const char* key, int idx, const char* kind) {
    if (!mMrb || !key || !kind) return;
    mrb_value all = mrb_gv_get(mMrb, mrb_intern_lit(mMrb, kRuiBlocksVar));
    if (!mrb_hash_p(all)) return;
    const std::string ks(key);
    mrb_value k = mrb_str_new(mMrb, ks.data(), (mrb_int)ks.size());
    mrb_value entry = mrb_hash_get(mMrb, all, k);
    if (!mrb_hash_p(entry)) return;
    mrb_value blk = mrb_hash_get(mMrb, entry, mrb_symbol_value(mrb_intern_lit(mMrb, kind)));
    if (mrb_nil_p(blk)) return;
    mrb_value arg = mrb_int_value(mMrb, idx);
    mrb_funcall_argv(mMrb, blk, mrb_intern_lit(mMrb, "call"), 1, &arg);
    if (mMrb->exc) {
        CaptureException("Rui-Block");
        mMrb->exc = nullptr; // Engine darf bei Ruby-Fehler nicht stehen bleiben
    }
}

void RubyVM::BindRui() {
    struct RClass* mod = mrb_define_module(mMrb, "Rui");
    g_rui.mod = mod;
    g_rui.win = mrb_define_class_under(mMrb, mod, "Window", mMrb->object_class);
    g_rui.label = mrb_define_class_under(mMrb, mod, "Label", mMrb->object_class);
    g_rui.gauge = mrb_define_class_under(mMrb, mod, "Gauge", mMrb->object_class);
    g_rui.list = mrb_define_class_under(mMrb, mod, "ListView", mMrb->object_class);

    // Modul-Funktionen
    mrb_define_module_function(mMrb, mod, "window", rb_rui_window_create, MRB_ARGS_REQ(5));
    mrb_define_module_function(mMrb, mod, "find", rb_rui_window_find, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, mod, "clear", rb_rui_clear, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, mod, "set_focus_list", rb_rui_set_focus_list, MRB_ARGS_REQ(2));
    mrb_define_module_function(mMrb, mod, "clear_focus", rb_rui_clear_focus, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, mod, "has_focus?", rb_rui_has_focus, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, mod, "windowskin=", rb_rui_windowskin_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, mod, "windowskin", rb_rui_windowskin_get, MRB_ARGS_NONE());

    // Rui::Window (Instanz-Methoden; Fenster-Handles, XP-Ergonomie)
    auto W = g_rui.win;
    mrb_define_method(mMrb, W, "id", rb_rui_win_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "move", rb_rui_win_move, MRB_ARGS_REQ(4));
    mrb_define_method(mMrb, W, "x", rb_rui_win_x, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "y", rb_rui_win_y, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "width", rb_rui_win_w, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "height", rb_rui_win_h, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "x=", rb_rui_win_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "y=", rb_rui_win_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "width=", rb_rui_win_w_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "height=", rb_rui_win_h_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "openness", rb_rui_win_openness, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "openness=", rb_rui_win_openness_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "open", rb_rui_win_open, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "close", rb_rui_win_close, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "closing?", rb_rui_win_closing_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "fully_open?", rb_rui_win_open_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "visible", rb_rui_win_visible, MRB_ARGS_NONE());
    mrb_define_method(mMrb, W, "visible=", rb_rui_win_visible_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "add_label", rb_rui_win_add_label, MRB_ARGS_REQ(2) | MRB_ARGS_OPT(4));
    mrb_define_method(mMrb, W, "add_gauge", rb_rui_win_add_gauge, MRB_ARGS_REQ(5) | MRB_ARGS_OPT(3));
    mrb_define_method(mMrb, W, "add_list", rb_rui_win_add_list, MRB_ARGS_REQ(2) | MRB_ARGS_OPT(4));
    mrb_define_method(mMrb, W, "remove_widget", rb_rui_win_remove_widget, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, W, "destroy", rb_rui_win_destroy, MRB_ARGS_NONE());

    // Rui::Label
    auto L = g_rui.label;
    mrb_define_method(mMrb, L, "text", rb_rui_label_text, MRB_ARGS_NONE());
    mrb_define_method(mMrb, L, "text=", rb_rui_label_text_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, L, "set_color", rb_rui_label_color_set, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(3));

    // Rui::Gauge
    auto G = g_rui.gauge;
    mrb_define_method(mMrb, G, "current", rb_rui_gauge_current, MRB_ARGS_NONE());
    mrb_define_method(mMrb, G, "current=", rb_rui_gauge_current_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, G, "maximum", rb_rui_gauge_maximum, MRB_ARGS_NONE());
    mrb_define_method(mMrb, G, "maximum=", rb_rui_gauge_maximum_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, G, "set_color", rb_rui_gauge_color_set, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(3));

    // Rui::ListView
    auto V = g_rui.list;
    mrb_define_method(mMrb, V, "selected", rb_rui_list_selected, MRB_ARGS_NONE());
    mrb_define_method(mMrb, V, "selected=", rb_rui_list_selected_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, V, "items=", rb_rui_list_items_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, V, "on_pick", rb_rui_list_on_pick, MRB_ARGS_BLOCK());
    mrb_define_method(mMrb, V, "on_cancel", rb_rui_list_on_cancel, MRB_ARGS_BLOCK());
    mrb_define_method(mMrb, V, "on_hover", rb_rui_list_on_hover, MRB_ARGS_BLOCK());
}

void RubyVM::BindRgssWindow() {
    struct RClass* win = mrb_define_class(mMrb, "Window", mMrb->object_class);
    mrb_define_method(mMrb, win, "initialize", rb_win_init, MRB_ARGS_OPT(4));
    mrb_define_method(mMrb, win, "x", rb_win_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "x=", rb_win_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "y", rb_win_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "y=", rb_win_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "z", rb_win_z_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "z=", rb_win_z_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "width", rb_win_width_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "width=", rb_win_width_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "height", rb_win_height_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "height=", rb_win_height_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "openness", rb_win_openness_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "openness=", rb_win_openness_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "visible", rb_win_visible_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "visible=", rb_win_visible_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "windowskin", rb_win_skin_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "windowskin=", rb_win_skin_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "text", rb_win_text_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "text=", rb_win_text_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "text_color=", rb_win_text_color_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "dispose", rb_win_dispose, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "disposed?", rb_win_disposed_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "update", rb_win_update, MRB_ARGS_NONE());

    struct RClass* rgss = mrb_define_module(mMrb, "RGSS");
    mrb_define_module_function(mMrb, rgss, "clear_windows", rb_rgss_clear_windows, MRB_ARGS_NONE());
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
    // XP-Bildschirme: Menue + Speicherbildschirm (4 Slots)
    mrb_define_module_function(mMrb, uiModule, "open_menu", rb_ui_open_menu, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "open_save_screen", rb_ui_open_save_screen, MRB_ARGS_OPT(1));
    mrb_define_module_function(mMrb, uiModule, "open_load_screen", rb_ui_open_load_screen, MRB_ARGS_NONE());
    // "Alles custom": eigene Menues per Block + Abschalten der Native-UIs
    mrb_define_module_function(mMrb, uiModule, "open_list_menu", rb_ui_open_list_menu, MRB_ARGS_REQ(2) | MRB_ARGS_BLOCK());
    mrb_define_module_function(mMrb, uiModule, "open_name_input", rb_ui_open_name_input, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2) | MRB_ARGS_BLOCK());
    mrb_define_module_function(mMrb, uiModule, "native_title=", rb_ui_native_title_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "native_title?", rb_ui_native_title_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "native_hud=", rb_ui_native_hud_set_live, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "native_hud?", rb_ui_native_hud_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "native_game_menu=", rb_ui_native_game_menu_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "native_game_menu?", rb_ui_native_game_menu_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "native_battle_menu=", rb_ui_native_battle_menu_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "native_battle_menu?", rb_ui_native_battle_menu_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, uiModule, "native_battle_status=", rb_ui_native_battle_status_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "native_battle_status?", rb_ui_native_battle_status_get, MRB_ARGS_NONE());
    // XP-Szenen-Framework (PAKET 6/h, Opt-in): UI.xp_scene_mode = true ->
    // die Engine tickt pro Frame $scene.__engine_frame (Scene_Base).
    mrb_define_module_function(mMrb, uiModule, "xp_scene_mode=", rb_ui_native_xp_scene_mode_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, uiModule, "xp_scene_mode?", rb_ui_native_xp_scene_mode_get, MRB_ARGS_NONE());

    // Custom-Start (eigener Titel ruft das auf "Neues Spiel" auf)
    {
        struct RClass* gm = mrb_define_class(mMrb, "Game", mMrb->object_class);
        mrb_define_module_function(mMrb, gm, "new_game", rb_game_new_game, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, gm, "start_game", rb_game_start_game, MRB_ARGS_NONE());
    }

    // ---------- Battle-Modul: Custom-Kampfszenen auf dem nativen Kern ----------
    {
        struct RClass* bm = mrb_define_module(mMrb, "Battle");
        mrb_define_const(mMrb, bm, "NONE",   mrb_int_value(mMrb, 0));
        mrb_define_const(mMrb, bm, "ATTACK", mrb_int_value(mMrb, 1));
        mrb_define_const(mMrb, bm, "GUARD",  mrb_int_value(mMrb, 2));
        mrb_define_const(mMrb, bm, "SKILL",  mrb_int_value(mMrb, 3));
        mrb_define_const(mMrb, bm, "ITEM",   mrb_int_value(mMrb, 4));
        mrb_define_const(mMrb, bm, "ESCAPE", mrb_int_value(mMrb, 5));
        mrb_define_module_function(mMrb, bm, "setup", rb_bm_setup, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2));
        mrb_define_module_function(mMrb, bm, "in_battle?", rb_battle_in_battle, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "needs_input?", rb_bm_needs_input, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "input_actor_index", rb_bm_input_actor_index, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "input_actor_id", rb_bm_input_actor_id, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "can_escape?", rb_bm_can_escape, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "turn", rb_bm_turn, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "last_outcome", rb_bm_last_outcome, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "last_exp", rb_bm_last_exp, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "last_gold", rb_bm_last_gold, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "actors", rb_bm_actors, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "enemies", rb_bm_enemies, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "set_action", rb_bm_set_action, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(4));
        mrb_define_module_function(mMrb, bm, "abort", rb_bm_abort, MRB_ARGS_NONE());
        mrb_define_module_function(mMrb, bm, "damage_enemy", rb_bm_damage_enemy, MRB_ARGS_REQ(2));
        mrb_define_module_function(mMrb, bm, "damage_actor", rb_bm_damage_actor, MRB_ARGS_REQ(2));
        mrb_define_module_function(mMrb, bm, "heal_enemy", rb_bm_heal_enemy, MRB_ARGS_REQ(2) | MRB_ARGS_OPT(1));
        mrb_define_module_function(mMrb, bm, "heal_actor", rb_bm_heal_actor, MRB_ARGS_REQ(2) | MRB_ARGS_OPT(1));
        mrb_define_module_function(mMrb, bm, "message", rb_bm_message, MRB_ARGS_REQ(1));
    }

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
    // ---------- XP Game_Event (PAKET 19) ----------
    // Wrapper-Klasse auf die nativen MapEvents des EventSystems — $game_map
    // .events liefert gecachte Instanzen (siehe rb_game_map_events).
    struct RClass* cGameEvent = mrb_define_class(mMrb, "Game_Event", mMrb->object_class);
    mrb_define_method(mMrb, cGameEvent, "initialize", rb_gev_initialize, MRB_ARGS_OPT(2));
    mrb_define_method(mMrb, cGameEvent, "map_id", rb_gev_map_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "id", rb_gev_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "valid?", rb_gev_valid, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "name", rb_gev_name, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "x", rb_gev_x, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "y", rb_gev_y, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "direction", rb_gev_direction, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "through", rb_gev_through_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "through=", rb_gev_through_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEvent, "transparent", rb_gev_transparent_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "transparent=", rb_gev_transparent_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEvent, "move_speed", rb_gev_move_speed_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "move_speed=", rb_gev_move_speed_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEvent, "moveto", rb_gev_moveto, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cGameEvent, "erase", rb_gev_erase, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "erased", rb_gev_erased, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "erased?", rb_gev_erased, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEvent, "refresh", rb_gev_refresh, MRB_ARGS_NONE());

    // XP Game_Map als vollwertige KLASSE (Stufe 4f): $game_map ist eine
    // Instanz, nicht mehr das Modul selbst — so wie XP-Skripte es erwarten
    // ($game_map.data[x, y, z], .display_x, .events, .need_refresh).
    struct RClass* gameMapClass = mrb_define_class(mMrb, "Game_Map", mMrb->object_class);
    mrb_define_method(mMrb, gameMapClass, "visible?", rb_game_map_visible, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "visible=", rb_game_map_set_visible, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, gameMapClass, "id", rb_game_map_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "map_id", rb_game_map_id, MRB_ARGS_NONE()); // XP-Name
    mrb_define_method(mMrb, gameMapClass, "setup", rb_game_map_setup, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, gameMapClass, "width", rb_game_map_width, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "height", rb_game_map_height, MRB_ARGS_NONE());
    // XP Game_Map-Flags (PAKET 6, XP_Scripts): Passage/Bush/Terrain-Tag
    mrb_define_method(mMrb, gameMapClass, "passable?", rb_game_map_passable, MRB_ARGS_ARG(2, 2));
    mrb_define_method(mMrb, gameMapClass, "bush?", rb_game_map_bush, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, gameMapClass, "terrain_tag", rb_game_map_terrain_tag, MRB_ARGS_REQ(2));
    // XP-Instanz-Zugang (Stufe 4f): Karte als Table, Scroll-State, Refresh
    mrb_define_method(mMrb, gameMapClass, "data", rb_game_map_data, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "display_x", rb_game_map_display_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "display_x=", rb_game_map_display_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, gameMapClass, "display_y", rb_game_map_display_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "display_y=", rb_game_map_display_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, gameMapClass, "events", rb_game_map_events, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "refresh", rb_game_map_refresh, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "need_refresh", rb_game_map_need_refresh_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, gameMapClass, "need_refresh=", rb_game_map_need_refresh_set, MRB_ARGS_REQ(1));

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
    // XP Game_Party-Ergaenzungen (Stufe 4g): item_number/... als XP-Namen
    // der Zaehler, has_item, all_dead?; interne ID-Listen fuer das
    // Prelude (actors/items/weapons/armors dort in Ruby aufgebaut).
    mrb_define_method(mMrb, cParty, "item_number", rb_gp_item_count, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "weapon_number", rb_gp_weapon_count, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "armor_number", rb_gp_armor_count, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "has_item", rb_gp_has_item, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cParty, "all_dead?", rb_gp_all_dead, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cParty, "__actor_ids", rb_gp_actor_ids, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cParty, "__item_ids", rb_gp_item_ids, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cParty, "__weapon_ids", rb_gp_weapon_ids, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cParty, "__armor_ids", rb_gp_armor_ids, MRB_ARGS_NONE());
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

    // $game_map = Game_Map.new — XP-Verhalten: Instanz mit der
    // Map-API (Need-Refresh-Semantik: Setzer loesen sofort aus).
    mrb_gv_set(mMrb, mrb_intern_lit(mMrb, "$game_map"),
               mrb_obj_new(mMrb, gameMapClass, 0, nullptr));

    // ---------- XP Game_Actor (Stufe 4g): Bruecke zur nativen Party-Struct
    // Die eigentliche Instanz-Sammlung $game_actors (mit Cache pro ID)
    // baut das Prelude in Ruby — hier nur die Klasse.
    struct RClass* cGameActor = mrb_define_class(mMrb, "Game_Actor", mMrb->object_class);
    mrb_define_method(mMrb, cGameActor, "initialize", rb_gactor_initialize, MRB_ARGS_OPT(1));
    mrb_define_method(mMrb, cGameActor, "id", rb_gactor_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "actor_id", rb_gactor_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "exist?", rb_gactor_exist, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "name", rb_gactor_name, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "name=", rb_gactor_name_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "class_id", rb_gactor_class_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "level", rb_gactor_level, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "level=", rb_gactor_level_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "exp", rb_gactor_exp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "exp=", rb_gactor_exp_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "next_exp", rb_gactor_next_exp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "add_exp", rb_gactor_add_exp, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "hp", rb_gactor_hp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "hp=", rb_gactor_hp_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "sp", rb_gactor_sp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "sp=", rb_gactor_sp_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "maxhp", rb_gactor_maxhp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "maxsp", rb_gactor_maxsp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "atk", rb_gactor_atk, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "def", rb_gactor_def, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "agi", rb_gactor_agi, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "dead?", rb_gactor_dead, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "recover_all", rb_gactor_recover_all, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "states", rb_gactor_states, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "add_state", rb_gactor_add_state, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "remove_state", rb_gactor_remove_state, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "skills", rb_gactor_skills, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "learn_skill", rb_gactor_learn_skill, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "forget_skill", rb_gactor_forget_skill, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "weapon_id", rb_gactor_weapon_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "armor1_id", rb_gactor_armor1_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "armor2_id", rb_gactor_armor2_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "armor3_id", rb_gactor_armor3_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "armor4_id", rb_gactor_armor4_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "character_name", rb_gactor_character_name, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "face_index", rb_gactor_face_index, MRB_ARGS_NONE());
    // Stufe 4g Teil 3: XP-Equip-Mutator (Inventar-Tausch = equip im Prelude)
    mrb_define_method(mMrb, cGameActor, "change_equip", rb_gactor_change_equip, MRB_ARGS_REQ(2));
    // Battler-Bildschirmposition im Kampf (XP Arrow_Actor; 0 ausserhalb)
    mrb_define_method(mMrb, cGameActor, "x", rb_gactor_x, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "y", rb_gactor_y, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "screen_x", rb_gactor_screen_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "screen_x=", rb_gactor_screen_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameActor, "screen_y", rb_gactor_screen_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameActor, "screen_y=", rb_gactor_screen_y_set, MRB_ARGS_REQ(1));

    // ---------- XP Game_Enemy (Stufe 4g Teil 3): live an den nativen ----------
    // Kampf gekoppelt (oder Orphan-Datenbankwerte). states kommen im Prelude
    // als Ruby-Ivars dazu (unser C++-Kampf kennt keine Gegner-Zustaende —
    // dokumentierte Grenze).
    struct RClass* cGameEnemy = mrb_define_class(mMrb, "Game_Enemy", mMrb->object_class);
    mrb_define_method(mMrb, cGameEnemy, "initialize", rb_genemy_initialize, MRB_ARGS_OPT(1));
    mrb_define_method(mMrb, cGameEnemy, "__attach", rb_genemy_attach, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEnemy, "id", rb_genemy_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "enemy_id", rb_genemy_id, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "index", rb_genemy_index, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "exist?", rb_genemy_exist, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "name", rb_genemy_name, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "battler_name", rb_genemy_battler_name, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "battler_hue", rb_genemy_battler_hue, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "hp", rb_genemy_hp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "hp=", rb_genemy_hp_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEnemy, "sp", rb_genemy_sp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "sp=", rb_genemy_sp_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEnemy, "maxhp", rb_genemy_maxhp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "maxsp", rb_genemy_maxsp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "atk", rb_genemy_atk, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "def", rb_genemy_def, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "agi", rb_genemy_agi, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "dead?", rb_genemy_dead, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "recover_all", rb_genemy_recover_all, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "exp", rb_genemy_exp, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "gold", rb_genemy_gold, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "transform", rb_genemy_transform, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEnemy, "animation1_id", rb_genemy_animation_zero, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "animation2_id", rb_genemy_animation_zero, MRB_ARGS_NONE());
    // Battler-Bildschirmposition (XP Arrow_*; Projektion via worldToScreenHook)
    mrb_define_method(mMrb, cGameEnemy, "x", rb_genemy_x, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "y", rb_genemy_y, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "screen_x", rb_genemy_screen_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "screen_x=", rb_genemy_screen_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, cGameEnemy, "screen_y", rb_genemy_screen_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameEnemy, "screen_y=", rb_genemy_screen_y_set, MRB_ARGS_REQ(1));

    // ---------- XP Game_Troop (Stufe 4g Teil 3): nur ID-Bruecke nativ ----------
    struct RClass* cGameTroop = mrb_define_class(mMrb, "Game_Troop", mMrb->object_class);
    mrb_define_method(mMrb, cGameTroop, "__enemy_ids", rb_gtroop_enemy_ids, MRB_ARGS_REQ(1));

    // ---------- XP Game_Screen (Stufe 4g Teil 3): direkt an ScreenEffects ----------
    // (Flash/Tone/Shake der Event-Befehle 223-225). pictures/weather baut
    // das Prelude in Ruby; Dauer-Uebergaben sind XP-Frames (40 fps).
    struct RClass* cGameScreen = mrb_define_class(mMrb, "Game_Screen", mMrb->object_class);
    mrb_define_method(mMrb, cGameScreen, "initialize", rb_gscreen_initialize, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameScreen, "start_flash", rb_gscreen_start_flash, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cGameScreen, "flash_color", rb_gscreen_flash_color, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameScreen, "start_tone_change", rb_gscreen_start_tone_change, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, cGameScreen, "tone", rb_gscreen_tone, MRB_ARGS_NONE());
    mrb_define_method(mMrb, cGameScreen, "start_shake", rb_gscreen_start_shake, MRB_ARGS_REQ(3));
    mrb_define_method(mMrb, cGameScreen, "shake", rb_gscreen_shake, MRB_ARGS_NONE());

    // ---------- XP-Bruecke (Stufe 4g): Kampfstart -> $game_troop.setup ----------
    // Feuert bei JEDEM Kampfstart (Skript- UND Event-Weg — die beiden
    // Trichter Game::StartBattleByTroop und EventSystem WireInterpreter
    // melden die Truppen-ID hierher). Fehler im Setup duerfen den
    // Kampfstart nicht abwetzen -> Exception wird geschluckt.
    Game::Get().onBattleStarted = [this](int troopId) {
        if (!mMrb) return;
        mrb_value troop = mrb_gv_get(mMrb, mrb_intern_lit(mMrb, "$game_troop"));
        if (mrb_nil_p(troop)) return;
        mrb_value arg = mrb_int_value(mMrb, (mrb_int)troopId);
        mrb_funcall_argv(mMrb, troop, mrb_intern_lit(mMrb, "setup"), 1, &arg);
        if (mMrb->exc) mMrb->exc = nullptr;
    };
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

bool RubyVM::CallGameHook(const std::string& name) { (void)name; return false; }
void RubyVM::CallListMenuBlock(int index) { (void)index; }
void RubyVM::CallRuiBlock(const char* key, int idx, const char* kind) { (void)key; (void)idx; (void)kind; }
void RubyVM::CallNameInputResult(const std::string& name) { (void)name; }

bool RubyVM::CaptureException(const std::string&) { return false; }

void RubyVM::BindEngine() {}
void RubyVM::BindInput() {}
void RubyVM::BindAudio() {}
void RubyVM::BindMap() {}
void RubyVM::BindActor() {}
void RubyVM::BindCamera() {}
void RubyVM::BindGame() {}
void RubyVM::BindUI() {}
void RubyVM::BindRui() {}
void RubyVM::BindRgssWindow() {}

} // namespace rpg

#endif // RPGMAKER3D_ENABLE_RUBY
