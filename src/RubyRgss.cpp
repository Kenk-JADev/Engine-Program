// ============================================================================
// RGSS-Bindings Teil 2 (RPG Maker XP-komplett):
//   Rect / Color / Tone / Font / Table / Bitmap / Viewport /
//   Sprite / Plane / Tilemap / Graphics / Input(XP) / Audio(XP) /
//   Window-Vollset + RGSS-Prelude-Lader
// ----------------------------------------------------------------------------
// Diese Uebersetzungseinheit spiegelt die privaten RubyVM::BindRgss*()-
// Funktionen; im Stub-Modus (ohne mruby) sind sie leer, siehe Dateiende.
// ============================================================================

#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/RgssUI.h"
#include "rpgmaker3d/RgssPrelude.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/EventSystem.h"

#include <cstdio>
#include <filesystem>

// ssize_t-Fix wie in RubyVM.cpp (MSVC kennt den POSIX-Typ nicht)
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#ifndef _SSIZE_T_DEFINED
typedef std::intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif
#else
#ifndef _SSIZE_T_DEFINED
#include <unistd.h>
#endif
#endif

#ifdef RPGMAKER3D_ENABLE_RUBY

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/value.h>
#include "rpgmaker3d/RubyCompat.h"

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

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>

namespace rpg {

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

// ID eines nativen RGSS-Objekts aus einer Ivar lesen (0 = keins/nil)
static int RgssObjId(mrb_state* mrb, mrb_value v, const char* ivarName) {
    if (mrb_nil_p(v) || mrb_immediate_p(v)) return 0;
    if (mrb_type(v) != MRB_TT_OBJECT) return 0;
    mrb_value idv = mrb_iv_get(mrb, v, mrb_intern_cstr(mrb, ivarName));
    return mrb_integer_p(idv) ? (int)mrb_integer(idv) : 0;
}

// Ruby-Wrapper-Objekt fuer eine bestehende native ID erzeugen
static mrb_value RgssWrapById(mrb_state* mrb, const char* className,
                              const char* ivarName, int id) {
    struct RClass* k = mrb_class_get(mrb, className);
    mrb_value obj = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_OBJECT, k));
    if (id > 0)
        mrb_iv_set(mrb, obj, mrb_intern_cstr(mrb, ivarName), RPG_MRB_INT_VALUE(mrb, id));
    return obj;
}

// Wert lesen: Integer oder Float -> float
static float RgssToFloat(mrb_state* mrb, mrb_value v) {
    if (mrb_float_p(v)) return (float)mrb_float(v);
    if (mrb_integer_p(v)) return (float)mrb_integer(v);
    return (float)mrb_as_float(mrb, v);
}

// "to_i"-Koerzierung (XP-Verhalten fuer Table o. a.)
static mrb_int RgssToInt(mrb_state* mrb, mrb_value v) {
    if (mrb_integer_p(v)) return mrb_integer(v);
    if (mrb_float_p(v)) return (mrb_int)mrb_float(v);
    mrb_value r = mrb_funcall(mrb, v, "to_i", 0);
    return mrb_integer_p(r) ? mrb_integer(r) : 0;
}

// Ruby-String (inkl. Konvertierung beliebiger Werte ueber to_s)
static std::string RgssToString(mrb_state* mrb, mrb_value v) {
    if (!mrb_string_p(v)) v = mrb_any_to_s(mrb, v);
    return std::string(RSTRING_PTR(v), (size_t)RSTRING_LEN(v));
}

// Optionales Rect-/Color-/Tone-Objekt auslesen (0 = nil/keins)
static int RgssRectIdOf(mrb_state* mrb, mrb_value v)  { return RgssObjId(mrb, v, "__rgss_rect_id"); }
static int RgssColorIdOf(mrb_state* mrb, mrb_value v) { return RgssObjId(mrb, v, "__rgss_color_id"); }
static int RgssToneIdOf(mrb_state* mrb, mrb_value v)  { return RgssObjId(mrb, v, "__rgss_tone_id"); }
static int RgssBmpIdOf(mrb_state* mrb, mrb_value v)   { return RgssObjId(mrb, v, "__rgss_bmp_id"); }
static int RgssTableIdOf(mrb_state* mrb, mrb_value v) { return RgssObjId(mrb, v, "__rgss_table_id"); }
static int RgssVpIdOf(mrb_state* mrb, mrb_value v)    { return RgssObjId(mrb, v, "__rgss_vp_id"); }
static int RgssDrwIdOf(mrb_state* mrb, mrb_value v)   { return RgssObjId(mrb, v, "__rgss_drw_id"); }
static int RgssWinIdOf(mrb_state* mrb, mrb_value v)   { return RgssObjId(mrb, v, "__rgss_id"); }

// ---------------------------------------------------------------------------
// Rect
// ---------------------------------------------------------------------------
static struct RgssRectState* RgssRectFrom(mrb_state* mrb, mrb_value self) {
    return RgssRectGet(RgssRectIdOf(mrb, self));
}

static mrb_value rb_rect_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0, y = 0, w = 0, h = 0;
    if (argc >= 4) {
        x = RgssToFloat(mrb, argv[0]); y = RgssToFloat(mrb, argv[1]);
        w = RgssToFloat(mrb, argv[2]); h = RgssToFloat(mrb, argv[3]);
    }
    // Rect.new(Rect) = Kopie
    if (argc == 1) {
        if (auto* o = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) {
            x = o->x; y = o->y; w = o->w; h = o->h;
        }
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_rect_id"),
               RPG_MRB_INT_VALUE(mrb, RgssRectCreate(x, y, w, h)));
    return self;
}

#define RGSS_RECT_FATTR(rname, field)                                              \
static mrb_value rb_rect_##rname##_get(mrb_state* mrb, mrb_value self) {          \
    if (auto* r = RgssRectFrom(mrb, self)) return mrb_float_value(mrb, r->field); \
    return mrb_float_value(mrb, 0);                                                \
}                                                                                 \
static mrb_value rb_rect_##rname##_set(mrb_state* mrb, mrb_value self) {          \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* r = RgssRectFrom(mrb, self)) r->field = (float)v;                   \
    return mrb_float_value(mrb, v);                                                \
}
RGSS_RECT_FATTR(x, x)
RGSS_RECT_FATTR(y, y)
RGSS_RECT_FATTR(width, w)
RGSS_RECT_FATTR(height, h)

static mrb_value rb_rect_set(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (argc >= 4) {
        if (auto* r = RgssRectFrom(mrb, self)) {
            r->x = RgssToFloat(mrb, argv[0]); r->y = RgssToFloat(mrb, argv[1]);
            r->w = RgssToFloat(mrb, argv[2]); r->h = RgssToFloat(mrb, argv[3]);
        }
    } else if (argc == 1) {
        if (auto* o = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) {
            if (auto* r = RgssRectFrom(mrb, self)) *r = *o;
        }
    }
    return self;
}

static mrb_value rb_rect_empty(mrb_state* mrb, mrb_value self) {
    if (auto* r = RgssRectFrom(mrb, self)) { r->x = r->y = r->w = r->h = 0; }
    return self;
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
static RgssColorState* RgssColorFrom(mrb_state* mrb, mrb_value self) {
    return RgssColorGet(RgssColorIdOf(mrb, self));
}

static mrb_value rb_color_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float r = 0, g = 0, b = 0, a = 0;
    if (argc >= 3) {
        r = RgssToFloat(mrb, argv[0]); g = RgssToFloat(mrb, argv[1]);
        b = RgssToFloat(mrb, argv[2]);
        if (argc >= 4) a = RgssToFloat(mrb, argv[3]);
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_color_id"),
               RPG_MRB_INT_VALUE(mrb, RgssColorCreate(r, g, b, a)));
    return self;
}

#define RGSS_COLOR_FATTR(rname, field)                                             \
static mrb_value rb_color_##rname##_get(mrb_state* mrb, mrb_value self) {         \
    if (auto* c = RgssColorFrom(mrb, self)) return mrb_float_value(mrb, c->field); \
    return mrb_float_value(mrb, 0);                                                \
}                                                                                 \
static mrb_value rb_color_##rname##_set(mrb_state* mrb, mrb_value self) {         \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* c = RgssColorFrom(mrb, self))                                       \
        c->field = std::clamp((float)v, 0.0f, 255.0f);                            \
    return mrb_float_value(mrb, v);                                                \
}
RGSS_COLOR_FATTR(red, r)
RGSS_COLOR_FATTR(green, g)
RGSS_COLOR_FATTR(blue, b)
RGSS_COLOR_FATTR(alpha, a)

static mrb_value rb_color_set(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (auto* c = RgssColorFrom(mrb, self)) {
        if (argc >= 3) {
            c->r = std::clamp(RgssToFloat(mrb, argv[0]), 0.0f, 255.0f);
            c->g = std::clamp(RgssToFloat(mrb, argv[1]), 0.0f, 255.0f);
            c->b = std::clamp(RgssToFloat(mrb, argv[2]), 0.0f, 255.0f);
            c->a = argc >= 4 ? std::clamp(RgssToFloat(mrb, argv[3]), 0.0f, 255.0f) : 255.0f;
        } else if (argc == 1) {
            if (auto* o = RgssColorGet(RgssColorIdOf(mrb, argv[0]))) *c = *o;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------
// Tone
// ---------------------------------------------------------------------------
static RgssToneState* RgssToneFrom(mrb_state* mrb, mrb_value self) {
    return RgssToneGet(RgssToneIdOf(mrb, self));
}

static mrb_value rb_tone_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float r = 0, g = 0, b = 0, gray = 0;
    if (argc >= 3) {
        r = RgssToFloat(mrb, argv[0]); g = RgssToFloat(mrb, argv[1]);
        b = RgssToFloat(mrb, argv[2]);
        if (argc >= 4) gray = RgssToFloat(mrb, argv[3]);
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_tone_id"),
               RPG_MRB_INT_VALUE(mrb, RgssToneCreate(r, g, b, gray)));
    return self;
}

#define RGSS_TONE_FATTR(rname, field)                                              \
static mrb_value rb_tone_##rname##_get(mrb_state* mrb, mrb_value self) {          \
    if (auto* t = RgssToneFrom(mrb, self)) return mrb_float_value(mrb, t->field); \
    return mrb_float_value(mrb, 0);                                                \
}                                                                                 \
static mrb_value rb_tone_##rname##_set(mrb_state* mrb, mrb_value self) {          \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* t = RgssToneFrom(mrb, self))                                        \
        t->field = std::clamp((float)v, -255.0f, 255.0f);                         \
    return mrb_float_value(mrb, v);                                                \
}
RGSS_TONE_FATTR(red, r)
RGSS_TONE_FATTR(green, g)
RGSS_TONE_FATTR(blue, b)
RGSS_TONE_FATTR(gray, gray)

static mrb_value rb_tone_set(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    const auto cl = [](float v) { return std::clamp(v, -255.0f, 255.0f); };
    if (auto* t = RgssToneFrom(mrb, self)) {
        if (argc >= 3) {
            t->r = cl(RgssToFloat(mrb, argv[0]));
            t->g = cl(RgssToFloat(mrb, argv[1]));
            t->b = cl(RgssToFloat(mrb, argv[2]));
            t->gray = argc >= 4 ? cl(RgssToFloat(mrb, argv[3])) : 0.0f;
        } else if (argc == 1) {
            if (auto* o = RgssToneGet(RgssToneIdOf(mrb, argv[0]))) *t = *o;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------
// Table (1-3 dim, int16)
// ---------------------------------------------------------------------------
static RgssTableState* RgssTableFrom(mrb_state* mrb, mrb_value self) {
    return RgssTableGet(RgssTableIdOf(mrb, self));
}

static mrb_value rb_table_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    int xs = 1, ys = 1, zs = 1;
    if (argc >= 1) xs = (int)RgssToInt(mrb, argv[0]);
    if (argc >= 2) ys = (int)RgssToInt(mrb, argv[1]);
    if (argc >= 3) zs = (int)RgssToInt(mrb, argv[2]);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_table_id"),
               RPG_MRB_INT_VALUE(mrb, RgssTableCreate(xs, ys, zs)));
    return self;
}

static int TableIndex(RgssTableState* t, int x, int y, int z) {
    x = std::clamp(x, 0, t->xs - 1);
    y = std::clamp(y, 0, t->ys - 1);
    z = std::clamp(z, 0, t->zs - 1);
    return ((z * t->ys) + y) * t->xs + x;
}

static mrb_value rb_table_get(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    auto* t = RgssTableFrom(mrb, self);
    if (!t || argc < 1) return RPG_MRB_INT_VALUE(mrb, 0);
    const int x = (int)RgssToInt(mrb, argv[0]);
    const int y = argc >= 2 ? (int)RgssToInt(mrb, argv[1]) : 0;
    const int z = argc >= 3 ? (int)RgssToInt(mrb, argv[2]) : 0;
    return RPG_MRB_INT_VALUE(mrb, t->data[TableIndex(t, x, y, z)]);
}

static mrb_value rb_table_set(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    auto* t = RgssTableFrom(mrb, self);
    if (!t || argc < 2) return RPG_MRB_INT_VALUE(mrb, 0);
    const int x = (int)RgssToInt(mrb, argv[0]);
    const int y = argc >= 3 ? (int)RgssToInt(mrb, argv[1]) : 0;
    const int z = argc >= 4 ? (int)RgssToInt(mrb, argv[2]) : 0;
    const mrb_int v = RgssToInt(mrb, argv[argc - 1]);
    const int16_t sv = (int16_t)(uint16_t)(v & 0xFFFF); // XP: Wrap-around
    t->data[TableIndex(t, x, y, z)] = sv;
    return RPG_MRB_INT_VALUE(mrb, sv);
}

#define RGSS_TABLE_SIZE(rname, field)                                             \
static mrb_value rb_table_##rname(mrb_state* mrb, mrb_value self) {              \
    if (auto* t = RgssTableFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, t->field);  \
    return RPG_MRB_INT_VALUE(mrb, 0);                                                  \
}
RGSS_TABLE_SIZE(xsize, xs)
RGSS_TABLE_SIZE(ysize, ys)
RGSS_TABLE_SIZE(zsize, zs)

static mrb_value rb_table_resize(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    auto* t = RgssTableFrom(mrb, self);
    if (!t || argc < 1) return self;
    const int nxs = std::max(1, (int)RgssToInt(mrb, argv[0]));
    const int nys = argc >= 2 ? std::max(1, (int)RgssToInt(mrb, argv[1])) : 1;
    const int nzs = argc >= 3 ? std::max(1, (int)RgssToInt(mrb, argv[2])) : 1;
    std::vector<int16_t> nd((size_t)nxs * nys * nzs, 0);
    for (int z = 0; z < nzs; ++z)
        for (int y = 0; y < nys; ++y)
            for (int x = 0; x < nxs; ++x) {
                const size_t di = ((size_t)z * nys + y) * nxs + x;
                if (x < t->xs && y < t->ys && z < t->zs)
                    nd[di] = t->data[((size_t)z * t->ys + y) * t->xs + x];
            }
    t->xs = nxs; t->ys = nys; t->zs = nzs;
    t->data = std::move(nd);
    return self;
}

// ---------------------------------------------------------------------------
// Font (Instanz-API; default_* laeuft ueber Prelude + FontDefaults-Modul)
// ---------------------------------------------------------------------------
static RgssFontState* RgssFontFrom(mrb_state* mrb, mrb_value self) {
    return RgssFontGet(RgssObjId(mrb, self, "__rgss_font_id"));
}

static mrb_value rb_font_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    const int id = RgssFontCreate();
    if (auto* f = RgssFontGet(id)) {
        if (argc >= 1 && !mrb_nil_p(argv[0])) {
            mrb_value n = argv[0];
            if (mrb_array_p(n)) n = RARRAY_LEN(n) > 0 ? mrb_ary_ref(mrb, n, 0) : n;
            if (mrb_string_p(n)) f->name = RgssToString(mrb, n);
        }
        if (argc >= 2) f->size = std::clamp((int)RgssToInt(mrb, argv[1]), 6, 96);
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_font_id"), RPG_MRB_INT_VALUE(mrb, id));
    return self;
}

static mrb_value rb_font_name_get(mrb_state* mrb, mrb_value self) {
    if (auto* f = RgssFontFrom(mrb, self))
        return mrb_str_new_cstr(mrb, f->name.c_str());
    return mrb_str_new_cstr(mrb, "");
}
static mrb_value rb_font_name_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* f = RgssFontFrom(mrb, self)) {
        if (mrb_array_p(v)) v = RARRAY_LEN(v) > 0 ? mrb_ary_ref(mrb, v, 0) : v;
        f->name = RgssToString(mrb, v);
    }
    return v;
}
static mrb_value rb_font_size_get(mrb_state* mrb, mrb_value self) {
    if (auto* f = RgssFontFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, f->size);
    return RPG_MRB_INT_VALUE(mrb, 22);
}
static mrb_value rb_font_size_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 22; mrb_get_args(mrb, "i", &v);
    if (auto* f = RgssFontFrom(mrb, self)) f->size = std::clamp((int)v, 6, 96);
    return RPG_MRB_INT_VALUE(mrb, v);
}
static mrb_value rb_font_bold_get(mrb_state* mrb, mrb_value self) {
    if (auto* f = RgssFontFrom(mrb, self)) return mrb_bool_value(f->bold);
    return mrb_bool_value(false);
}
static mrb_value rb_font_bold_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    if (auto* f = RgssFontFrom(mrb, self)) f->bold = v;
    return mrb_bool_value(v);
}
static mrb_value rb_font_italic_get(mrb_state* mrb, mrb_value self) {
    if (auto* f = RgssFontFrom(mrb, self)) return mrb_bool_value(f->italic);
    return mrb_bool_value(false);
}
static mrb_value rb_font_italic_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    if (auto* f = RgssFontFrom(mrb, self)) f->italic = v;
    return mrb_bool_value(v);
}
static mrb_value rb_font_color_get(mrb_state* mrb, mrb_value self) {
    if (auto* f = RgssFontFrom(mrb, self))
        return RgssWrapById(mrb, "Color", "__rgss_color_id", f->colorId);
    return mrb_nil_value();
}
static mrb_value rb_font_color_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* f = RgssFontFrom(mrb, self)) {
        const int cid = RgssColorIdOf(mrb, v);
        if (cid > 0) f->colorId = cid; // Referenz-Semantik (geteilt!)
    }
    return v;
}

// FontDefaults-Modul (Rueckgrat der prelude-seitigen Font.default_*-API)
static mrb_value rb_fdef_name_get(mrb_state* mrb, mrb_value) {
    return mrb_str_new_cstr(mrb, RgssFontDefaults().name.c_str());
}
static mrb_value rb_fdef_name_set(mrb_state* mrb, mrb_value) {
    mrb_value v; mrb_get_args(mrb, "o", &v);
    if (mrb_array_p(v)) v = RARRAY_LEN(v) > 0 ? mrb_ary_ref(mrb, v, 0) : v;
    RgssFontDefaults().name = RgssToString(mrb, v);
    return v;
}
static mrb_value rb_fdef_size_get(mrb_state* mrb, mrb_value) {
    (void)mrb; return RPG_MRB_INT_VALUE(mrb, RgssFontDefaults().size);
}
static mrb_value rb_fdef_size_set(mrb_state* mrb, mrb_value) {
    mrb_int v = 22; mrb_get_args(mrb, "i", &v);
    RgssFontDefaults().size = std::clamp((int)v, 6, 96);
    return RPG_MRB_INT_VALUE(mrb, v);
}
static mrb_value rb_fdef_bold_get(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(RgssFontDefaults().bold);
}
static mrb_value rb_fdef_bold_set(mrb_state* mrb, mrb_value) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    RgssFontDefaults().bold = v;
    return mrb_bool_value(v);
}
static mrb_value rb_fdef_italic_get(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(RgssFontDefaults().italic);
}
static mrb_value rb_fdef_italic_set(mrb_state* mrb, mrb_value) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    RgssFontDefaults().italic = v;
    return mrb_bool_value(v);
}
static mrb_value rb_fdef_color_get(mrb_state* mrb, mrb_value) {
    return RgssWrapById(mrb, "Color", "__rgss_color_id", RgssFontDefaults().colorId);
}
static mrb_value rb_fdef_color_set(mrb_state* mrb, mrb_value) {
    mrb_value v; mrb_get_args(mrb, "o", &v);
    const int cid = RgssColorIdOf(mrb, v);
    if (cid > 0) RgssFontDefaults().colorId = cid;
    return v;
}

// ---------------------------------------------------------------------------
// Bitmap
// ---------------------------------------------------------------------------
static RgssBitmapState* RgssBmpFrom(mrb_state* mrb, mrb_value self) {
    return RgssBmpGet(RgssBmpIdOf(mrb, self));
}

static mrb_value rb_bmp_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    int id = 0;
    if (argc == 1 && mrb_string_p(argv[0])) {
        const std::string path = RgssToString(mrb, argv[0]);
        id = RgssBmpLoad(path);
        if (id == 0) {
            const std::string msg = "RGSS: Bitmap nicht gefunden: " + path;
            mrb_raise(mrb, E_RUNTIME_ERROR, msg.c_str());
        }
    } else if (argc >= 2) {
        id = RgssBmpCreate(std::max(1, (int)RgssToInt(mrb, argv[0])),
                           std::max(1, (int)RgssToInt(mrb, argv[1])));
    } else {
        mrb_raise(mrb, E_ARGUMENT_ERROR,
                  "Bitmap.new erwartet (filename) oder (width, height)");
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_bmp_id"), RPG_MRB_INT_VALUE(mrb, id));
    return self;
}

static mrb_value rb_bmp_wrap(mrb_state* mrb, int id) {
    return RgssWrapById(mrb, "Bitmap", "__rgss_bmp_id", id);
}

static mrb_value rb_bmp_dispose(mrb_state* mrb, mrb_value self) {
    RgssBmpDispose(RgssBmpIdOf(mrb, self));
    return mrb_nil_value();
}
static mrb_value rb_bmp_disposed_p(mrb_state* mrb, mrb_value self) {
    auto* b = RgssBmpFrom(mrb, self);
    return mrb_bool_value(!b || b->disposed);
}
static mrb_value rb_bmp_width(mrb_state* mrb, mrb_value self) {
    auto* b = RgssBmpFrom(mrb, self);
    return RPG_MRB_INT_VALUE(mrb, b ? b->width : 0);
}
static mrb_value rb_bmp_height(mrb_state* mrb, mrb_value self) {
    auto* b = RgssBmpFrom(mrb, self);
    return RPG_MRB_INT_VALUE(mrb, b ? b->height : 0);
}
static mrb_value rb_bmp_rect(mrb_state* mrb, mrb_value self) {
    auto* b = RgssBmpFrom(mrb, self);
    if (!b) return mrb_nil_value();
    return RgssWrapById(mrb, "Rect", "__rgss_rect_id",
                        RgssRectCreate(0, 0, (float)b->width, (float)b->height));
}

static mrb_value rb_bmp_blt(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (argc >= 4) {
        const int srcId = RgssBmpIdOf(mrb, argv[2]);
        const mrb_value r = argv[3];
        int sx = 0, sy = 0, sw = 0, sh = 0;
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, r))) {
            sx = (int)rr->x; sy = (int)rr->y; sw = (int)rr->w; sh = (int)rr->h;
        }
        const float op = argc >= 5 ? RgssToFloat(mrb, argv[4]) : 255.0f;
        RgssBmpBlt(RgssBmpIdOf(mrb, self),
                   RgssToFloat(mrb, argv[0]), RgssToFloat(mrb, argv[1]),
                   srcId, (float)sx, (float)sy, (float)sw, (float)sh, op);
    }
    return mrb_nil_value();
}

static mrb_value rb_bmp_stretch_blt(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (argc >= 3) {
        float dx = 0, dy = 0, dw = 0, dh = 0;
        if (auto* dr = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) {
            dx = dr->x; dy = dr->y; dw = dr->w; dh = dr->h;
        }
        const int srcId = RgssBmpIdOf(mrb, argv[1]);
        float sx = 0, sy = 0, sw = 0, sh = 0;
        if (auto* sr = RgssRectGet(RgssRectIdOf(mrb, argv[2]))) {
            sx = sr->x; sy = sr->y; sw = sr->w; sh = sr->h;
        }
        const float op = argc >= 4 ? RgssToFloat(mrb, argv[3]) : 255.0f;
        RgssBmpStretchBlt(RgssBmpIdOf(mrb, self), dx, dy, dw, dh,
                          srcId, sx, sy, sw, sh, op);
    }
    return mrb_nil_value();
}

static mrb_value rb_bmp_fill_rect(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0, y = 0, w = 0, h = 0, cr = 0, cg = 0, cb = 0, ca = 0;
    if (argc >= 5) {
        x = RgssToFloat(mrb, argv[0]); y = RgssToFloat(mrb, argv[1]);
        w = RgssToFloat(mrb, argv[2]); h = RgssToFloat(mrb, argv[3]);
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[4]))) {
            cr = c->r; cg = c->g; cb = c->b; ca = c->a;
        }
    } else if (argc >= 2) {
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) {
            x = rr->x; y = rr->y; w = rr->w; h = rr->h;
        }
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[1]))) {
            cr = c->r; cg = c->g; cb = c->b; ca = c->a;
        }
    }
    RgssBmpFillRect(RgssBmpIdOf(mrb, self), x, y, w, h, cr, cg, cb, ca);
    return mrb_nil_value();
}

static mrb_value rb_bmp_gradient_fill_rect(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0, y = 0, w = 0, h = 0;
    float r1 = 0, g1 = 0, b1 = 0, a1 = 0, r2 = 0, g2 = 0, b2 = 0, a2 = 0;
    bool vertical = false;
    if (argc >= 6) { // (x, y, w, h, color1, color2[, vertical])
        x = RgssToFloat(mrb, argv[0]); y = RgssToFloat(mrb, argv[1]);
        w = RgssToFloat(mrb, argv[2]); h = RgssToFloat(mrb, argv[3]);
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[4]))) { r1 = c->r; g1 = c->g; b1 = c->b; a1 = c->a; }
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[5]))) { r2 = c->r; g2 = c->g; b2 = c->b; a2 = c->a; }
        if (argc >= 7) vertical = !mrb_nil_p(argv[6]) && !(mrb_type(argv[6]) == MRB_TT_FALSE);
    } else if (argc >= 3) { // (rect, color1, color2[, vertical])
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) { x = rr->x; y = rr->y; w = rr->w; h = rr->h; }
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[1]))) { r1 = c->r; g1 = c->g; b1 = c->b; a1 = c->a; }
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[2]))) { r2 = c->r; g2 = c->g; b2 = c->b; a2 = c->a; }
        if (argc >= 4) vertical = !mrb_nil_p(argv[3]) && !(mrb_type(argv[3]) == MRB_TT_FALSE);
    }
    RgssBmpGradientFillRect(RgssBmpIdOf(mrb, self), x, y, w, h,
                            r1, g1, b1, a1, r2, g2, b2, a2, vertical);
    return mrb_nil_value();
}

static mrb_value rb_bmp_clear(mrb_state* mrb, mrb_value self) {
    RgssBmpClear(RgssBmpIdOf(mrb, self));
    return mrb_nil_value();
}
static mrb_value rb_bmp_clear_rect(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (argc >= 4) {
        RgssBmpClearRect(RgssBmpIdOf(mrb, self),
                         RgssToFloat(mrb, argv[0]), RgssToFloat(mrb, argv[1]),
                         RgssToFloat(mrb, argv[2]), RgssToFloat(mrb, argv[3]));
    } else if (argc >= 1) {
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, argv[0])))
            RgssBmpClearRect(RgssBmpIdOf(mrb, self), rr->x, rr->y, rr->w, rr->h);
    }
    return mrb_nil_value();
}

static mrb_value rb_bmp_get_pixel(mrb_state* mrb, mrb_value self) {
    mrb_int x = 0, y = 0;
    mrb_get_args(mrb, "ii", &x, &y);
    float r = 0, g = 0, b = 0, a = 0;
    RgssBmpGetPixel(RgssBmpIdOf(mrb, self), (int)x, (int)y, r, g, b, a);
    return RgssWrapById(mrb, "Color", "__rgss_color_id",
                        RgssColorCreate(r, g, b, a));
}
static mrb_value rb_bmp_set_pixel(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (argc >= 3) {
        float r = 0, g = 0, b = 0, a = 0;
        if (auto* c = RgssColorGet(RgssColorIdOf(mrb, argv[2]))) {
            r = c->r; g = c->g; b = c->b; a = c->a;
        }
        RgssBmpSetPixel(RgssBmpIdOf(mrb, self),
                        (int)RgssToInt(mrb, argv[0]), (int)RgssToInt(mrb, argv[1]),
                        r, g, b, a);
    }
    return mrb_nil_value();
}

static mrb_value rb_bmp_hue_change(mrb_state* mrb, mrb_value self) {
    mrb_float h = 0; mrb_get_args(mrb, "f", &h);
    RgssBmpHueChange(RgssBmpIdOf(mrb, self), (float)h);
    return mrb_nil_value();
}
static mrb_value rb_bmp_blur(mrb_state* mrb, mrb_value self) {
    (void)mrb;
    RgssBmpBlur(RgssBmpIdOf(mrb, self));
    return mrb_nil_value();
}
static mrb_value rb_bmp_radial_blur(mrb_state* mrb, mrb_value self) {
    mrb_float a = 0; mrb_int d = 2;
    mrb_get_args(mrb, "fi", &a, &d);
    RgssBmpRadialBlur(RgssBmpIdOf(mrb, self), (float)a, (int)std::clamp(d, (mrb_int)2, (mrb_int)100));
    return mrb_nil_value();
}

static mrb_value rb_bmp_draw_text(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0, y = 0, w = 0, h = 0;
    std::string str;
    int align = 0;
    if (argc >= 5) { // draw_text(x, y, width, height, str[, align])
        x = RgssToFloat(mrb, argv[0]); y = RgssToFloat(mrb, argv[1]);
        w = RgssToFloat(mrb, argv[2]); h = RgssToFloat(mrb, argv[3]);
        str = RgssToString(mrb, argv[4]);
        if (argc >= 6) align = (int)RgssToInt(mrb, argv[5]);
    } else if (argc >= 2) { // draw_text(rect, str[, align])
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) {
            x = rr->x; y = rr->y; w = rr->w; h = rr->h;
        }
        str = RgssToString(mrb, argv[1]);
        if (argc >= 3) align = (int)RgssToInt(mrb, argv[2]);
    } else {
        mrb_raise(mrb, E_ARGUMENT_ERROR,
                  "draw_text(x,y,w,h,str[,align]) oder draw_text(rect,str[,align])");
    }
    RgssBmpDrawText(RgssBmpIdOf(mrb, self), x, y, w, h, str, align);
    return mrb_nil_value();
}

static mrb_value rb_bmp_text_size(mrb_state* mrb, mrb_value self) {
    mrb_value s;
    mrb_get_args(mrb, "o", &s);
    float w = 0, h = 0;
    RgssBmpTextSize(RgssBmpIdOf(mrb, self), RgssToString(mrb, s), w, h);
    return RgssWrapById(mrb, "Rect", "__rgss_rect_id",
                        RgssRectCreate(0, 0, w, h));
}

static mrb_value rb_bmp_font_get(mrb_state* mrb, mrb_value self) {
    auto* b = RgssBmpFrom(mrb, self);
    if (!b) return mrb_nil_value();
    return RgssWrapById(mrb, "Font", "__rgss_font_id", b->fontId);
}
static mrb_value rb_bmp_font_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* b = RgssBmpFrom(mrb, self)) {
        const int fid = RgssObjId(mrb, v, "__rgss_font_id");
        if (fid > 0) b->fontId = fid;
    }
    return v;
}

static mrb_value rb_bmp_clone(mrb_state* mrb, mrb_value self) {
    (void)mrb;
    const int nid = RgssBmpClone(RgssBmpIdOf(mrb, self));
    if (nid <= 0) return mrb_nil_value();
    return rb_bmp_wrap(mrb, nid);
}

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------
static RgssViewportState* RgssVpFrom(mrb_state* mrb, mrb_value self) {
    return RgssVpGet(RgssVpIdOf(mrb, self));
}

static mrb_value rb_vp_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0, y = 0, w = 0, h = 0;
    if (argc >= 4) {
        x = RgssToFloat(mrb, argv[0]); y = RgssToFloat(mrb, argv[1]);
        w = RgssToFloat(mrb, argv[2]); h = RgssToFloat(mrb, argv[3]);
    } else if (argc == 1) {
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, argv[0]))) {
            x = rr->x; y = rr->y; w = rr->w; h = rr->h;
        }
    }
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_vp_id"),
               RPG_MRB_INT_VALUE(mrb, RgssVpCreate(x, y, w, h)));
    return self;
}

static mrb_value rb_vp_dispose(mrb_state* mrb, mrb_value self) {
    RgssVpDispose(RgssVpIdOf(mrb, self));
    return mrb_nil_value();
}
static mrb_value rb_vp_disposed_p(mrb_state* mrb, mrb_value self) {
    auto* v = RgssVpFrom(mrb, self);
    return mrb_bool_value(!v || v->disposed);
}
static mrb_value rb_vp_rect_get(mrb_state* mrb, mrb_value self) {
    if (auto* v = RgssVpFrom(mrb, self))
        return RgssWrapById(mrb, "Rect", "__rgss_rect_id",
                            RgssRectCreate(v->x, v->y, v->w, v->h));
    return mrb_nil_value();
}
static mrb_value rb_vp_rect_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* vp = RgssVpFrom(mrb, self))
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, v))) {
            vp->x = rr->x; vp->y = rr->y; vp->w = rr->w; vp->h = rr->h;
        }
    return v;
}
static mrb_value rb_vp_visible_get(mrb_state* mrb, mrb_value self) {
    if (auto* v = RgssVpFrom(mrb, self)) return mrb_bool_value(v->visible);
    return mrb_bool_value(false);
}
static mrb_value rb_vp_visible_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    if (auto* vp = RgssVpFrom(mrb, self)) vp->visible = v;
    return mrb_bool_value(v);
}
static mrb_value rb_vp_z_get(mrb_state* mrb, mrb_value self) {
    if (auto* v = RgssVpFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, v->z);
    return RPG_MRB_INT_VALUE(mrb, 0);
}
static mrb_value rb_vp_z_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* vp = RgssVpFrom(mrb, self)) vp->z = (int)v;
    return RPG_MRB_INT_VALUE(mrb, v);
}

#define RGSS_VP_FATTR(rname, field)                                                \
static mrb_value rb_vp_##rname##_get(mrb_state* mrb, mrb_value self) {            \
    if (auto* v = RgssVpFrom(mrb, self)) return mrb_float_value(mrb, v->field);   \
    return mrb_float_value(mrb, 0);                                                \
}                                                                                 \
static mrb_value rb_vp_##rname##_set(mrb_state* mrb, mrb_value self) {            \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* vp = RgssVpFrom(mrb, self)) vp->field = (float)v;                   \
    return mrb_float_value(mrb, v);                                                \
}
RGSS_VP_FATTR(ox, ox)
RGSS_VP_FATTR(oy, oy)

static mrb_value rb_vp_color_get(mrb_state* mrb, mrb_value self) {
    if (auto* v = RgssVpFrom(mrb, self); v && v->colorId > 0)
        return RgssWrapById(mrb, "Color", "__rgss_color_id", v->colorId);
    return mrb_nil_value();
}
static mrb_value rb_vp_color_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* vp = RgssVpFrom(mrb, self)) vp->colorId = RgssColorIdOf(mrb, v);
    return v;
}
static mrb_value rb_vp_tone_get(mrb_state* mrb, mrb_value self) {
    if (auto* v = RgssVpFrom(mrb, self); v && v->toneId > 0)
        return RgssWrapById(mrb, "Tone", "__rgss_tone_id", v->toneId);
    return mrb_nil_value();
}
static mrb_value rb_vp_tone_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* vp = RgssVpFrom(mrb, self)) vp->toneId = RgssToneIdOf(mrb, v);
    return v;
}
static mrb_value rb_vp_flash(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (auto* vp = RgssVpFrom(mrb, self)) {
        if (argc >= 1 && mrb_nil_p(argv[0])) {
            vp->flashHide = true;
            vp->flashColorId = 0;
        } else {
            vp->flashHide = false;
            vp->flashColorId = argc >= 1 ? RgssColorIdOf(mrb, argv[0]) : 0;
        }
        vp->flashDuration = argc >= 2 ? std::max(0, (int)RgssToInt(mrb, argv[1])) : 0;
        vp->flashTimer = vp->flashDuration;
    }
    return mrb_nil_value();
}
static mrb_value rb_vp_update(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self; // Flash tickt frame-getrieben; update() = Kompat-No-op
    return mrb_nil_value();
}

// ---------------------------------------------------------------------------
// Drawable-Basis (Sprite + Plane + Tilemap teilen Basis-Properties)
// ---------------------------------------------------------------------------
static RgssDrawableState* RgssDrwFrom(mrb_state* mrb, mrb_value self) {
    return RgssDrawableGet(RgssDrwIdOf(mrb, self));
}

static mrb_value rb_drw_dispose(mrb_state* mrb, mrb_value self) {
    RgssDrawableDispose(RgssDrwIdOf(mrb, self));
    return mrb_nil_value();
}
static mrb_value rb_drw_disposed_p(mrb_state* mrb, mrb_value self) {
    auto* d = RgssDrwFrom(mrb, self);
    return mrb_bool_value(!d || d->disposed);
}
static mrb_value rb_drw_viewport_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->viewportId > 0)
        return RgssWrapById(mrb, "Viewport", "__rgss_vp_id", d->viewportId);
    return mrb_nil_value();
}
static mrb_value rb_drw_visible_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self)) return mrb_bool_value(d->visible);
    return mrb_bool_value(false);
}
static mrb_value rb_drw_visible_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->visible = v;
    return mrb_bool_value(v);
}
static mrb_value rb_drw_z_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, d->z);
    return RPG_MRB_INT_VALUE(mrb, 0);
}
static mrb_value rb_drw_z_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->z = (int)v;
    return RPG_MRB_INT_VALUE(mrb, v);
}

#define RGSS_DRW_FATTR(rname, field)                                               \
static mrb_value rb_drw_##rname##_get(mrb_state* mrb, mrb_value self) {           \
    if (auto* d = RgssDrwFrom(mrb, self)) return mrb_float_value(mrb, d->field);  \
    return mrb_float_value(mrb, 0);                                                \
}                                                                                 \
static mrb_value rb_drw_##rname##_set(mrb_state* mrb, mrb_value self) {           \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* d = RgssDrwFrom(mrb, self)) d->field = (float)v;                    \
    return mrb_float_value(mrb, v);                                                \
}
RGSS_DRW_FATTR(x, x)
RGSS_DRW_FATTR(y, y)
RGSS_DRW_FATTR(ox, ox)
RGSS_DRW_FATTR(oy, oy)
RGSS_DRW_FATTR(zoom_x, zoomX)
RGSS_DRW_FATTR(zoom_y, zoomY)
RGSS_DRW_FATTR(angle, angle)
RGSS_DRW_FATTR(bush_depth, bushDepth)

static mrb_value rb_drw_mirror_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self)) return mrb_bool_value(d->mirror);
    return mrb_bool_value(false);
}
static mrb_value rb_drw_mirror_set(mrb_state* mrb, mrb_value self) {
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->mirror = v;
    return mrb_bool_value(v);
}
static mrb_value rb_drw_opacity_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, d->opacity);
    return RPG_MRB_INT_VALUE(mrb, 0);
}
static mrb_value rb_drw_opacity_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->opacity = std::clamp((int)v, 0, 255);
    return RPG_MRB_INT_VALUE(mrb, v);
}
static mrb_value rb_drw_blend_type_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self)) return RPG_MRB_INT_VALUE(mrb, d->blendType);
    return RPG_MRB_INT_VALUE(mrb, 0);
}
static mrb_value rb_drw_blend_type_set(mrb_state* mrb, mrb_value self) {
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->blendType = std::clamp((int)v, 0, 2);
    return RPG_MRB_INT_VALUE(mrb, v);
}
static mrb_value rb_drw_color_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->colorId > 0)
        return RgssWrapById(mrb, "Color", "__rgss_color_id", d->colorId);
    return mrb_nil_value();
}
static mrb_value rb_drw_color_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->colorId = RgssColorIdOf(mrb, v);
    return v;
}
static mrb_value rb_drw_tone_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->toneId > 0)
        return RgssWrapById(mrb, "Tone", "__rgss_tone_id", d->toneId);
    return mrb_nil_value();
}
static mrb_value rb_drw_tone_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->toneId = RgssToneIdOf(mrb, v);
    return v;
}
static mrb_value rb_drw_bitmap_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->bitmapId > 0)
        return rb_bmp_wrap(mrb, d->bitmapId);
    return mrb_nil_value();
}
static mrb_value rb_drw_bitmap_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->bitmapId = RgssBmpIdOf(mrb, v);
    return v;
}

// Sprite-only
static mrb_value rb_sprite_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    const int vpId = argc >= 1 ? RgssVpIdOf(mrb, argv[0]) : 0;
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_drw_id"),
               RPG_MRB_INT_VALUE(mrb, RgssDrawableCreate(RgssDrawableType::Sprite, vpId)));
    return self;
}
static mrb_value rb_sprite_src_rect_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self))
        return RgssWrapById(mrb, "Rect", "__rgss_rect_id",
                            RgssRectCreate(d->srcX, d->srcY, d->srcW, d->srcH));
    return mrb_nil_value();
}
static mrb_value rb_sprite_src_rect_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self))
        if (auto* rr = RgssRectGet(RgssRectIdOf(mrb, v))) {
            d->srcX = rr->x; d->srcY = rr->y; d->srcW = rr->w; d->srcH = rr->h;
        }
    return v;
}
static mrb_value rb_sprite_flash(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (auto* d = RgssDrwFrom(mrb, self)) {
        if (argc >= 1 && mrb_nil_p(argv[0])) {
            d->flashHide = true; d->flashColorId = 0;
        } else {
            d->flashHide = false;
            d->flashColorId = argc >= 1 ? RgssColorIdOf(mrb, argv[0]) : 0;
        }
        d->flashDuration = argc >= 2 ? std::max(0, (int)RgssToInt(mrb, argv[1])) : 0;
        d->flashTimer = d->flashDuration;
    }
    return mrb_nil_value();
}
static mrb_value rb_sprite_update(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_nil_value();
}

// Plane-only
static mrb_value rb_plane_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    const int vpId = argc >= 1 ? RgssVpIdOf(mrb, argv[0]) : 0;
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_drw_id"),
               RPG_MRB_INT_VALUE(mrb, RgssDrawableCreate(RgssDrawableType::Plane, vpId)));
    return self;
}

// Tilemap-only
static mrb_value rb_tilemap_init(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    const int vpId = argc >= 1 ? RgssVpIdOf(mrb, argv[0]) : 0;
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_drw_id"),
               RPG_MRB_INT_VALUE(mrb, RgssDrawableCreate(RgssDrawableType::Tilemap, vpId)));
    return self;
}
static mrb_value rb_tilemap_tileset_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->tilesetBmpId > 0)
        return rb_bmp_wrap(mrb, d->tilesetBmpId);
    return mrb_nil_value();
}
static mrb_value rb_tilemap_tileset_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->tilesetBmpId = RgssBmpIdOf(mrb, v);
    return v;
}
static mrb_value rb_tilemap_mapdata_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->mapDataId > 0)
        return RgssWrapById(mrb, "Table", "__rgss_table_id", d->mapDataId);
    return mrb_nil_value();
}
static mrb_value rb_tilemap_mapdata_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->mapDataId = RgssTableIdOf(mrb, v);
    return v;
}
static mrb_value rb_tilemap_flashdata_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->flashDataId > 0)
        return RgssWrapById(mrb, "Table", "__rgss_table_id", d->flashDataId);
    return mrb_nil_value();
}
static mrb_value rb_tilemap_flashdata_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->flashDataId = RgssTableIdOf(mrb, v);
    return v;
}
static mrb_value rb_tilemap_priorities_get(mrb_state* mrb, mrb_value self) {
    if (auto* d = RgssDrwFrom(mrb, self); d && d->prioritiesId > 0)
        return RgssWrapById(mrb, "Table", "__rgss_table_id", d->prioritiesId);
    return mrb_nil_value();
}
static mrb_value rb_tilemap_priorities_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* d = RgssDrwFrom(mrb, self)) d->prioritiesId = RgssTableIdOf(mrb, v);
    return v;
}
// autotiles-Proxy (XP: tilemap.autotiles[i] / =)
static mrb_value rb_atproxy_get(mrb_state* mrb, mrb_value self) {
    mrb_int idx = 0;
    mrb_get_args(mrb, "i", &idx);
    const int tmId = RgssObjId(mrb, self, "__rgss_tm_id");
    if (auto* d = RgssDrawableGet(tmId)) {
        if (idx >= 0 && idx < 7 && d->autotileBmpId[idx] > 0)
            return rb_bmp_wrap(mrb, d->autotileBmpId[idx]);
    }
    return mrb_nil_value();
}
static mrb_value rb_atproxy_set(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    if (argc >= 2) {
        const int tmId = RgssObjId(mrb, self, "__rgss_tm_id");
        if (auto* d = RgssDrawableGet(tmId)) {
            const mrb_int idx = RgssToInt(mrb, argv[0]);
            if (idx >= 0 && idx < 7) d->autotileBmpId[idx] = RgssBmpIdOf(mrb, argv[1]);
        }
        return argv[1];
    }
    return mrb_nil_value();
}
static mrb_value rb_tilemap_autotiles(mrb_state* mrb, mrb_value self) {
    mrb_value proxy = RgssWrapById(mrb, "TilemapAutotiles", "__rgss_tm_id",
                                   RgssDrwIdOf(mrb, self));
    return proxy;
}

// ---------------------------------------------------------------------------
// Window-Vollset (Klasse "Window" existiert bereits aus RubyVM.cpp)
// ---------------------------------------------------------------------------
static RgssWindowState* RgssWinFrom2(mrb_state* mrb, mrb_value self) {
    return RgssUI::Get().GetWindow(RgssWinIdOf(mrb, self));
}

static mrb_value rb_win_init_xp(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0, y = 0, w = 0, h = 0;
    int vpId = 0;
    if (argc >= 4) {
        x = RgssToFloat(mrb, argv[0]); y = RgssToFloat(mrb, argv[1]);
        w = RgssToFloat(mrb, argv[2]); h = RgssToFloat(mrb, argv[3]);
    } else if (argc == 1) {
        vpId = RgssVpIdOf(mrb, argv[0]);
    }
    const int id = RgssUI::Get().MakeWindow(x, y, w, h);
    if (auto* win = RgssUI::Get().GetWindow(id)) win->viewportId = vpId;
    // XP-Startwerte
    if (auto* win = RgssUI::Get().GetWindow(id)) win->openness = 255.0f;
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "__rgss_id"), RPG_MRB_INT_VALUE(mrb, id));
    return self;
}

static mrb_value rb_win_viewport_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom2(mrb, self); w && w->viewportId > 0)
        return RgssWrapById(mrb, "Viewport", "__rgss_vp_id", w->viewportId);
    return mrb_nil_value();
}

static mrb_value rb_win_windowskin_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom2(mrb, self)) {
        if (w->windowskinBmpId > 0) return rb_bmp_wrap(mrb, w->windowskinBmpId);
        if (!w->windowskin.empty()) return mrb_str_new_cstr(mrb, w->windowskin.c_str());
    }
    return mrb_nil_value();
}
static mrb_value rb_win_windowskin_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* w = RgssWinFrom2(mrb, self)) {
        if (mrb_string_p(v)) {           // Legacy-Komfort: Skin-Name als String
            w->windowskin = RgssToString(mrb, v);
            w->windowskinBmpId = 0;
        } else {
            w->windowskinBmpId = RgssBmpIdOf(mrb, v);
            if (w->windowskinBmpId > 0) w->windowskin.clear();
        }
    }
    return v;
}

static mrb_value rb_win_contents_get(mrb_state* mrb, mrb_value self) {
    auto* w = RgssWinFrom2(mrb, self);
    if (!w) return mrb_nil_value();
    if (w->contentsBmpId <= 0) {
        const int cw = std::max(1, (int)w->width - 32);
        const int ch = std::max(1, (int)w->height - 32);
        w->contentsBmpId = RgssBmpCreate(cw, ch);
    }
    return rb_bmp_wrap(mrb, w->contentsBmpId);
}
static mrb_value rb_win_contents_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* w = RgssWinFrom2(mrb, self)) w->contentsBmpId = RgssBmpIdOf(mrb, v);
    return v;
}

static mrb_value rb_win_cursor_rect_get(mrb_state* mrb, mrb_value self) {
    if (auto* w = RgssWinFrom2(mrb, self))
        if (auto* cr = RgssRectGet(w->cursorRectId))
            return RgssWrapById(mrb, "Rect", "__rgss_rect_id", w->cursorRectId);
    // noch keins: leeres Rect anlegen + merken
    if (auto* w = RgssWinFrom2(mrb, self)) {
        w->cursorRectId = RgssRectCreate(0, 0, 0, 0);
        return RgssWrapById(mrb, "Rect", "__rgss_rect_id", w->cursorRectId);
    }
    return mrb_nil_value();
}
static mrb_value rb_win_cursor_rect_set(mrb_state* mrb, mrb_value self) {
    mrb_value v;
    mrb_get_args(mrb, "o", &v);
    if (auto* w = RgssWinFrom2(mrb, self)) {
        if (w->cursorRectId <= 0) w->cursorRectId = RgssRectCreate(0, 0, 0, 0);
        if (auto* dst = RgssRectGet(w->cursorRectId))
            if (auto* src = RgssRectGet(RgssRectIdOf(mrb, v))) *dst = *src;
    }
    return v;
}

#define RGSS_WIN_BPROP(rname, field)                                               \
static mrb_value rb_win_##rname##_get(mrb_state* mrb, mrb_value self) {           \
    if (auto* w = RgssWinFrom2(mrb, self)) return mrb_bool_value(w->field);       \
    return mrb_bool_value(false);                                                  \
}                                                                                 \
static mrb_value rb_win_##rname##_set(mrb_state* mrb, mrb_value self) {           \
    mrb_bool v = false; mrb_get_args(mrb, "b", &v);                               \
    if (auto* w = RgssWinFrom2(mrb, self)) w->field = v;                          \
    return mrb_bool_value(v);                                                      \
}
RGSS_WIN_BPROP(active, active)
RGSS_WIN_BPROP(pause, pause)
RGSS_WIN_BPROP(stretch, stretch)

#define RGSS_WIN_IPROP(rname, field)                                               \
static mrb_value rb_win_##rname##_get(mrb_state* mrb, mrb_value self) {           \
    if (auto* w = RgssWinFrom2(mrb, self)) return RPG_MRB_INT_VALUE(mrb, w->field);    \
    return RPG_MRB_INT_VALUE(mrb, 0);                                                   \
}                                                                                 \
static mrb_value rb_win_##rname##_set(mrb_state* mrb, mrb_value self) {           \
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);                                    \
    if (auto* w = RgssWinFrom2(mrb, self))                                        \
        w->field = std::clamp((int)v, 0, 255);                                    \
    return RPG_MRB_INT_VALUE(mrb, v);                                                   \
}
RGSS_WIN_IPROP(opacity, opacity)
RGSS_WIN_IPROP(back_opacity, backOpacity)
RGSS_WIN_IPROP(contents_opacity, contentsOpacity)

#define RGSS_WIN_CFATTR(rname, field)                                              \
static mrb_value rb_win_##rname##_get(mrb_state* mrb, mrb_value self) {           \
    if (auto* w = RgssWinFrom2(mrb, self)) return mrb_float_value(mrb, w->field); \
    return mrb_float_value(mrb, 0);                                                \
}                                                                                 \
static mrb_value rb_win_##rname##_set(mrb_state* mrb, mrb_value self) {           \
    mrb_float v = 0; mrb_get_args(mrb, "f", &v);                                  \
    if (auto* w = RgssWinFrom2(mrb, self)) w->field = (float)v;                   \
    return mrb_float_value(mrb, v);                                                \
}
RGSS_WIN_CFATTR(ox, contentsOx)
RGSS_WIN_CFATTR(oy, contentsOy)

static mrb_value rb_win_update_xp(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self; // Cursor/Pause blinken frame-getrieben
    return mrb_nil_value();
}

// ---------------------------------------------------------------------------
// Graphics-Modul
// ---------------------------------------------------------------------------
static mrb_value rb_graphics_update(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    // Framebasierte Engine: Der Bildschnitt laeuft im Hauptloop; update()
    // ist Kompatibilitaets-No-op (XP-Szenenschleifen: SceneManager/$game.update).
    return mrb_nil_value();
}
static mrb_value rb_graphics_freeze(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    RgssGraphicsFreeze();
    return mrb_nil_value();
}
static mrb_value rb_graphics_transition(mrb_state* mrb, mrb_value self) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    int duration = 8;
    std::string file;
    float vague = 40.0f;
    if (argc >= 1) duration = (int)RgssToInt(mrb, argv[0]);
    if (argc >= 2 && !mrb_nil_p(argv[1])) file = RgssToString(mrb, argv[1]);
    if (argc >= 3) vague = RgssToFloat(mrb, argv[2]);
    RgssGraphicsTransition(duration, file, vague);
    return mrb_nil_value();
}
static mrb_value rb_graphics_frame_reset(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return mrb_nil_value();
}
static mrb_value rb_graphics_frame_rate_get(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return RPG_MRB_INT_VALUE(mrb, RgssGraphics().frameRate);
}
static mrb_value rb_graphics_frame_rate_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int v = 40; mrb_get_args(mrb, "i", &v);
    RgssGraphics().frameRate = std::clamp((int)v, 10, 120);
    return RPG_MRB_INT_VALUE(mrb, v);
}
static mrb_value rb_graphics_frame_count_get(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return RPG_MRB_INT_VALUE(mrb, (mrb_int)RgssGraphics().frameCount);
}
static mrb_value rb_graphics_frame_count_set(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int v = 0; mrb_get_args(mrb, "i", &v);
    RgssGraphics().frameCount = v;
    return RPG_MRB_INT_VALUE(mrb, v);
}
static mrb_value rb_graphics_width(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return RPG_MRB_INT_VALUE(mrb, 640);
}
static mrb_value rb_graphics_height(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return RPG_MRB_INT_VALUE(mrb, 480);
}

// ---------------------------------------------------------------------------
// Input (XP-Befehlssatz: press?/trigger?/repeat?/dir4/dir8 + Konstanten)
// ---------------------------------------------------------------------------
namespace {
long long s_inputFrame = -1;
int s_hold[32] = {};

bool XpKeyDown(mrb_state* mrb, int num) {
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (!engine) return false;
    Input& in = engine->GetInput();
    const auto anyDown = [&](std::initializer_list<Key> ks) {
        for (Key k : ks) if (in.IsKeyDown(k)) return true;
        return false;
    };
    switch (num) {
        case 2:  return anyDown({ Key::Down, Key::Num2 });
        case 4:  return anyDown({ Key::Left, Key::Num4 });
        case 6:  return anyDown({ Key::Right, Key::Num6 });
        case 8:  return anyDown({ Key::Up, Key::Num8 });
        case 11: return anyDown({ Key::LShift, Key::Z });       // A
        case 12: return anyDown({ Key::Escape, Key::X, Key::Num0 }); // B
        case 13: return anyDown({ Key::Space, Key::Enter, Key::C }); // C
        case 14: return anyDown({ Key::A });                    // X
        case 15: return anyDown({ Key::S });                    // Y
        case 16: return anyDown({ Key::D });                    // Z
        case 17: return anyDown({ Key::Q });                    // L
        case 18: return anyDown({ Key::W });                    // R
        case 21: return in.IsKeyDown(Key::LShift);
        case 22: return in.IsKeyDown(Key::LCtrl);
        case 23: return in.IsKeyDown(Key::LAlt);
        case 25: return in.IsKeyDown(Key::F5);
        case 26: return in.IsKeyDown(Key::F6);
        case 27: return in.IsKeyDown(Key::F7);
        case 28: return in.IsKeyDown(Key::F8);
        case 29: return in.IsKeyDown(Key::F9);
        default: return false;
    }
}

void XpInputTick(mrb_state* mrb) {
    const long long f = (long long)RgssGraphics().frameCount;
    if (f == s_inputFrame) return;
    s_inputFrame = f;
    for (int b = 0; b < 32; ++b) {
        const bool down = (b >= 2) && XpKeyDown(mrb, b);
        s_hold[b] = down ? s_hold[b] + 1 : 0;
    }
}
} // anonymous namespace

static mrb_value rb_input_update(mrb_state* mrb, mrb_value self) {
    (void)self;
    XpInputTick(mrb);
    return mrb_nil_value();
}
static mrb_value rb_input_press_p(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int num = 0;
    mrb_get_args(mrb, "i", &num);
    XpInputTick(mrb);
    return mrb_bool_value(XpKeyDown(mrb, (int)num));
}
static mrb_value rb_input_trigger_p(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int num = 0;
    mrb_get_args(mrb, "i", &num);
    XpInputTick(mrb);
    if (num >= 0 && num < 32) {
        const bool down = XpKeyDown(mrb, (int)num);
        return mrb_bool_value(down && s_hold[num] <= 1);
    }
    return mrb_bool_value(false);
}
static mrb_value rb_input_repeat_p(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_int num = 0;
    mrb_get_args(mrb, "i", &num);
    XpInputTick(mrb);
    if (num >= 0 && num < 32) {
        const int h = s_hold[num];
        if (!XpKeyDown(mrb, (int)num)) return mrb_bool_value(false);
        return mrb_bool_value(h == 1 || (h >= 16 && (h - 16) % 4 == 0));
    }
    return mrb_bool_value(false);
}
static mrb_value rb_input_dir4(mrb_state* mrb, mrb_value self) {
    (void)self;
    XpInputTick(mrb);
    if (XpKeyDown(mrb, 8)) return RPG_MRB_INT_VALUE(mrb, 8);
    if (XpKeyDown(mrb, 6)) return RPG_MRB_INT_VALUE(mrb, 6);
    if (XpKeyDown(mrb, 4)) return RPG_MRB_INT_VALUE(mrb, 4);
    if (XpKeyDown(mrb, 2)) return RPG_MRB_INT_VALUE(mrb, 2);
    return RPG_MRB_INT_VALUE(mrb, 0);
}
static mrb_value rb_input_dir8(mrb_state* mrb, mrb_value self) {
    (void)self;
    XpInputTick(mrb);
    const bool u = XpKeyDown(mrb, 8), d = XpKeyDown(mrb, 2);
    const bool l = XpKeyDown(mrb, 4), r = XpKeyDown(mrb, 6);
    if (u) { if (l) return RPG_MRB_INT_VALUE(mrb, 7); if (r) return RPG_MRB_INT_VALUE(mrb, 9); return RPG_MRB_INT_VALUE(mrb, 8); }
    if (d) { if (l) return RPG_MRB_INT_VALUE(mrb, 1); if (r) return RPG_MRB_INT_VALUE(mrb, 3); return RPG_MRB_INT_VALUE(mrb, 2); }
    if (l) return RPG_MRB_INT_VALUE(mrb, 4);
    if (r) return RPG_MRB_INT_VALUE(mrb, 6);
    return RPG_MRB_INT_VALUE(mrb, 0);
}

// ---------------------------------------------------------------------------
// Audio (XP-Signaturen: *_play(filename[, volume[, pitch]]), *_fade(ms), *_stop)
// ---------------------------------------------------------------------------
namespace {
SoundHandle s_lastME{};
bool s_meActive = false;
SoundHandle s_seRing[16] = {};
int s_seRingPos = 0;

// Dateiname oder RPG::AudioFile-Objekt -> (name, volume, pitch)
void RgssAudioArg(mrb_state* mrb, mrb_value v, std::string& name,
                  float& volume, float& pitch) {
    volume = 100.0f; pitch = 100.0f;
    if (mrb_string_p(v)) {
        name = RgssToString(mrb, v);
        return;
    }
    if (mrb_nil_p(v)) return;
    if (mrb_respond_to(mrb, v, mrb_intern_lit(mrb, "name"))) {
        mrb_value n = mrb_funcall(mrb, v, "name", 0);
        name = mrb_string_p(n) ? RgssToString(mrb, n) : std::string();
        mrb_value vv = mrb_funcall(mrb, v, "volume", 0);
        if (mrb_integer_p(vv) || mrb_float_p(vv)) volume = RgssToFloat(mrb, vv);
        mrb_value pv = mrb_funcall(mrb, v, "pitch", 0);
        if (mrb_integer_p(pv) || mrb_float_p(pv)) pitch = RgssToFloat(mrb, pv);
    }
}

// XP format: (filename|AudioFile[, volume[, pitch]])
struct RgssAudioArgs { std::string name; float volume = 100, pitch = 100; };
RgssAudioArgs RgssParseAudio(mrb_state* mrb) {
    const mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    RgssAudioArgs out;
    if (argc >= 1) RgssAudioArg(mrb, argv[0], out.name, out.volume, out.pitch);
    if (argc >= 2) out.volume = RgssToFloat(mrb, argv[1]);
    if (argc >= 3) out.pitch = RgssToFloat(mrb, argv[2]);
    out.volume = std::clamp(out.volume, 0.0f, 100.0f) / 100.0f;
    out.pitch = std::clamp(out.pitch, 50.0f, 150.0f) / 100.0f;
    return out;
}
} // anonymous namespace

static mrb_value rb_audio_bgm_play_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    RgssAudioArgs a = RgssParseAudio(mrb);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine && !a.name.empty()) {
        const std::string path = engine->ResolveAudioPath(a.name, 0);
        if (!path.empty()) engine->GetAudio().PlayBGM(path, true, a.volume, a.pitch);
        else RPG_LOG_WARN("[Audio] BGM nicht gefunden: " + a.name);
    }
    return mrb_nil_value();
}
static mrb_value rb_audio_bgs_play_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    RgssAudioArgs a = RgssParseAudio(mrb);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine && !a.name.empty()) {
        const std::string path = engine->ResolveAudioPath(a.name, 1);
        if (!path.empty()) engine->GetAudio().PlayBGS(path, true, a.volume, a.pitch);
        else RPG_LOG_WARN("[Audio] BGS nicht gefunden: " + a.name);
    }
    return mrb_nil_value();
}
static mrb_value rb_audio_me_play_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    RgssAudioArgs a = RgssParseAudio(mrb);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine && !a.name.empty()) {
        const std::string path = engine->ResolveAudioPath(a.name, 2);
        if (!path.empty()) {
            s_lastME = engine->GetAudio().PlayME(path, false, a.volume, a.pitch);
            s_meActive = true;
        } else RPG_LOG_WARN("[Audio] ME nicht gefunden: " + a.name);
    }
    return mrb_nil_value();
}
static mrb_value rb_audio_se_play_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    RgssAudioArgs a = RgssParseAudio(mrb);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine && !a.name.empty()) {
        const std::string path = engine->ResolveAudioPath(a.name, 3);
        if (!path.empty()) {
            SoundHandle h = engine->GetAudio().PlaySE(path, false, a.volume, a.pitch);
            s_seRing[s_seRingPos] = h;
            s_seRingPos = (s_seRingPos + 1) % 16;
        } else RPG_LOG_WARN("[Audio] SE nicht gefunden: " + a.name);
    }
    return mrb_nil_value();
}
static mrb_value rb_audio_bgm_stop_xp(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetAudio().FadeOutBGM(0.0f);
    return mrb_nil_value();
}
static mrb_value rb_audio_bgs_stop_xp(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetAudio().FadeOutBGS(0.0f);
    return mrb_nil_value();
}
static mrb_value rb_audio_me_stop_xp(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine && s_meActive) {
        engine->GetAudio().Stop(s_lastME, 0.0f);
        s_meActive = false;
    }
    return mrb_nil_value();
}
static mrb_value rb_audio_se_stop_xp(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) {
        for (auto& h : s_seRing) engine->GetAudio().Stop(h, 0.05f);
    }
    return mrb_nil_value();
}
static mrb_value rb_audio_bgm_fade_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_float ms = 0; mrb_get_args(mrb, "f", &ms);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetAudio().FadeOutBGM((float)ms / 1000.0f);
    return mrb_nil_value();
}
static mrb_value rb_audio_bgs_fade_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_float ms = 0; mrb_get_args(mrb, "f", &ms);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine) engine->GetAudio().FadeOutBGS((float)ms / 1000.0f);
    return mrb_nil_value();
}
static mrb_value rb_audio_me_fade_xp(mrb_state* mrb, mrb_value self) {
    (void)self;
    mrb_float ms = 0; mrb_get_args(mrb, "f", &ms);
    Engine* engine = static_cast<Engine*>(mrb->ud);
    if (engine && s_meActive) {
        engine->GetAudio().Stop(s_lastME, (float)ms / 1000.0f);
        s_meActive = false;
    }
    return mrb_nil_value();
}

// ---------------------------------------------------------------------------
// Bind-Funktionen (RubyVM-Methoden)
// ---------------------------------------------------------------------------
void RubyVM::BindRgssObjects() {
    // Rect
    struct RClass* rect = mrb_define_class(mMrb, "Rect", mMrb->object_class);
    mrb_define_method(mMrb, rect, "initialize", rb_rect_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, rect, "x", rb_rect_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, rect, "x=", rb_rect_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, rect, "y", rb_rect_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, rect, "y=", rb_rect_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, rect, "width", rb_rect_width_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, rect, "width=", rb_rect_width_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, rect, "height", rb_rect_height_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, rect, "height=", rb_rect_height_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, rect, "set", rb_rect_set, MRB_ARGS_ANY());
    mrb_define_method(mMrb, rect, "empty", rb_rect_empty, MRB_ARGS_NONE());

    // Color
    struct RClass* color = mrb_define_class(mMrb, "Color", mMrb->object_class);
    mrb_define_method(mMrb, color, "initialize", rb_color_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, color, "red", rb_color_red_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, color, "red=", rb_color_red_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, color, "green", rb_color_green_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, color, "green=", rb_color_green_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, color, "blue", rb_color_blue_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, color, "blue=", rb_color_blue_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, color, "alpha", rb_color_alpha_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, color, "alpha=", rb_color_alpha_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, color, "set", rb_color_set, MRB_ARGS_ANY());

    // Tone
    struct RClass* tone = mrb_define_class(mMrb, "Tone", mMrb->object_class);
    mrb_define_method(mMrb, tone, "initialize", rb_tone_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, tone, "red", rb_tone_red_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tone, "red=", rb_tone_red_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tone, "green", rb_tone_green_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tone, "green=", rb_tone_green_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tone, "blue", rb_tone_blue_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tone, "blue=", rb_tone_blue_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tone, "gray", rb_tone_gray_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tone, "gray=", rb_tone_gray_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tone, "set", rb_tone_set, MRB_ARGS_ANY());

    // Table
    struct RClass* table = mrb_define_class(mMrb, "Table", mMrb->object_class);
    mrb_define_method(mMrb, table, "initialize", rb_table_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, table, "[]", rb_table_get, MRB_ARGS_ANY());
    mrb_define_method(mMrb, table, "[]=", rb_table_set, MRB_ARGS_ANY());
    mrb_define_method(mMrb, table, "xsize", rb_table_xsize, MRB_ARGS_NONE());
    mrb_define_method(mMrb, table, "ysize", rb_table_ysize, MRB_ARGS_NONE());
    mrb_define_method(mMrb, table, "zsize", rb_table_zsize, MRB_ARGS_NONE());
    mrb_define_method(mMrb, table, "resize", rb_table_resize, MRB_ARGS_ANY());

    // Font
    struct RClass* font = mrb_define_class(mMrb, "Font", mMrb->object_class);
    mrb_define_method(mMrb, font, "initialize", rb_font_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, font, "name", rb_font_name_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, font, "name=", rb_font_name_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, font, "size", rb_font_size_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, font, "size=", rb_font_size_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, font, "bold", rb_font_bold_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, font, "bold=", rb_font_bold_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, font, "italic", rb_font_italic_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, font, "italic=", rb_font_italic_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, font, "color", rb_font_color_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, font, "color=", rb_font_color_set, MRB_ARGS_REQ(1));

    // FontDefaults (Rueckgrat fuer Font.default_* aus der Prelude)
    struct RClass* fdef = mrb_define_module(mMrb, "FontDefaults");
    mrb_define_module_function(mMrb, fdef, "name", rb_fdef_name_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, fdef, "name=", rb_fdef_name_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, fdef, "size", rb_fdef_size_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, fdef, "size=", rb_fdef_size_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, fdef, "bold", rb_fdef_bold_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, fdef, "bold=", rb_fdef_bold_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, fdef, "italic", rb_fdef_italic_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, fdef, "italic=", rb_fdef_italic_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, fdef, "color", rb_fdef_color_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, fdef, "color=", rb_fdef_color_set, MRB_ARGS_REQ(1));

    // Bitmap
    struct RClass* bmp = mrb_define_class(mMrb, "Bitmap", mMrb->object_class);
    mrb_define_method(mMrb, bmp, "initialize", rb_bmp_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "dispose", rb_bmp_dispose, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "disposed?", rb_bmp_disposed_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "width", rb_bmp_width, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "height", rb_bmp_height, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "rect", rb_bmp_rect, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "blt", rb_bmp_blt, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "stretch_blt", rb_bmp_stretch_blt, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "fill_rect", rb_bmp_fill_rect, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "gradient_fill_rect", rb_bmp_gradient_fill_rect, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "clear", rb_bmp_clear, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "clear_rect", rb_bmp_clear_rect, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "get_pixel", rb_bmp_get_pixel, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, bmp, "set_pixel", rb_bmp_set_pixel, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "hue_change", rb_bmp_hue_change, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, bmp, "blur", rb_bmp_blur, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "radial_blur", rb_bmp_radial_blur, MRB_ARGS_REQ(2));
    mrb_define_method(mMrb, bmp, "draw_text", rb_bmp_draw_text, MRB_ARGS_ANY());
    mrb_define_method(mMrb, bmp, "text_size", rb_bmp_text_size, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, bmp, "font", rb_bmp_font_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "font=", rb_bmp_font_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, bmp, "clone", rb_bmp_clone, MRB_ARGS_NONE());
    mrb_define_method(mMrb, bmp, "dup", rb_bmp_clone, MRB_ARGS_NONE());

    // Viewport
    struct RClass* vp = mrb_define_class(mMrb, "Viewport", mMrb->object_class);
    mrb_define_method(mMrb, vp, "initialize", rb_vp_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, vp, "dispose", rb_vp_dispose, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "disposed?", rb_vp_disposed_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "rect", rb_vp_rect_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "rect=", rb_vp_rect_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "visible", rb_vp_visible_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "visible=", rb_vp_visible_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "z", rb_vp_z_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "z=", rb_vp_z_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "ox", rb_vp_ox_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "ox=", rb_vp_ox_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "oy", rb_vp_oy_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "oy=", rb_vp_oy_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "color", rb_vp_color_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "color=", rb_vp_color_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "tone", rb_vp_tone_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, vp, "tone=", rb_vp_tone_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, vp, "flash", rb_vp_flash, MRB_ARGS_ANY());
    mrb_define_method(mMrb, vp, "update", rb_vp_update, MRB_ARGS_NONE());
}

void RubyVM::BindRgssDrawables() {
    // Sprite
    struct RClass* sprite = mrb_define_class(mMrb, "Sprite", mMrb->object_class);
    mrb_define_method(mMrb, sprite, "initialize", rb_sprite_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, sprite, "dispose", rb_drw_dispose, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "disposed?", rb_drw_disposed_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "viewport", rb_drw_viewport_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "bitmap", rb_drw_bitmap_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "bitmap=", rb_drw_bitmap_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "src_rect", rb_sprite_src_rect_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "src_rect=", rb_sprite_src_rect_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "visible", rb_drw_visible_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "visible=", rb_drw_visible_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "x", rb_drw_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "x=", rb_drw_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "y", rb_drw_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "y=", rb_drw_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "z", rb_drw_z_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "z=", rb_drw_z_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "ox", rb_drw_ox_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "ox=", rb_drw_ox_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "oy", rb_drw_oy_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "oy=", rb_drw_oy_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "zoom_x", rb_drw_zoom_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "zoom_x=", rb_drw_zoom_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "zoom_y", rb_drw_zoom_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "zoom_y=", rb_drw_zoom_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "angle", rb_drw_angle_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "angle=", rb_drw_angle_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "mirror", rb_drw_mirror_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "mirror=", rb_drw_mirror_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "bush_depth", rb_drw_bush_depth_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "bush_depth=", rb_drw_bush_depth_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "opacity", rb_drw_opacity_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "opacity=", rb_drw_opacity_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "blend_type", rb_drw_blend_type_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "blend_type=", rb_drw_blend_type_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "color", rb_drw_color_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "color=", rb_drw_color_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "tone", rb_drw_tone_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, sprite, "tone=", rb_drw_tone_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, sprite, "flash", rb_sprite_flash, MRB_ARGS_ANY());
    mrb_define_method(mMrb, sprite, "update", rb_sprite_update, MRB_ARGS_NONE());

    // Plane
    struct RClass* plane = mrb_define_class(mMrb, "Plane", mMrb->object_class);
    mrb_define_method(mMrb, plane, "initialize", rb_plane_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, plane, "dispose", rb_drw_dispose, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "disposed?", rb_drw_disposed_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "viewport", rb_drw_viewport_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "bitmap", rb_drw_bitmap_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "bitmap=", rb_drw_bitmap_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "visible", rb_drw_visible_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "visible=", rb_drw_visible_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "z", rb_drw_z_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "z=", rb_drw_z_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "ox", rb_drw_ox_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "ox=", rb_drw_ox_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "oy", rb_drw_oy_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "oy=", rb_drw_oy_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "zoom_x", rb_drw_zoom_x_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "zoom_x=", rb_drw_zoom_x_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "zoom_y", rb_drw_zoom_y_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "zoom_y=", rb_drw_zoom_y_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "opacity", rb_drw_opacity_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "opacity=", rb_drw_opacity_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "blend_type", rb_drw_blend_type_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "blend_type=", rb_drw_blend_type_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "color", rb_drw_color_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "color=", rb_drw_color_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, plane, "tone", rb_drw_tone_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, plane, "tone=", rb_drw_tone_set, MRB_ARGS_REQ(1));

    // Tilemap (+ Autotiles-Proxy)
    struct RClass* atproxy = mrb_define_class(mMrb, "TilemapAutotiles", mMrb->object_class);
    mrb_define_method(mMrb, atproxy, "[]", rb_atproxy_get, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, atproxy, "[]=", rb_atproxy_set, MRB_ARGS_ANY());

    struct RClass* tilemap = mrb_define_class(mMrb, "Tilemap", mMrb->object_class);
    mrb_define_method(mMrb, tilemap, "initialize", rb_tilemap_init, MRB_ARGS_ANY());
    mrb_define_method(mMrb, tilemap, "dispose", rb_drw_dispose, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "disposed?", rb_drw_disposed_p, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "viewport", rb_drw_viewport_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "tileset", rb_tilemap_tileset_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "tileset=", rb_tilemap_tileset_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "autotiles", rb_tilemap_autotiles, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "map_data", rb_tilemap_mapdata_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "map_data=", rb_tilemap_mapdata_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "flash_data", rb_tilemap_flashdata_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "flash_data=", rb_tilemap_flashdata_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "priorities", rb_tilemap_priorities_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "priorities=", rb_tilemap_priorities_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "visible", rb_drw_visible_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "visible=", rb_drw_visible_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "ox", rb_drw_ox_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "ox=", rb_drw_ox_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "oy", rb_drw_oy_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, tilemap, "oy=", rb_drw_oy_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, tilemap, "update", rb_sprite_update, MRB_ARGS_NONE());
}

// ---------------------------------------------------------------------------
// XP load_data-Bruecke (PAKET 6, XP_Scripts schrittweise): C++ liefert die
// Engine-Datenbank (JSON) als generische Ruby-Hashes; der Prelude baut
// daraus RPG::*-Objekte. Dadurch funktioniert load_data("Data/X.rxdata")
// der Original-XP-Skripte gegen unsere JSON-Datenbank. Der XP-Index-0-nil-
// Shift passiert auf der Ruby-Seite. Kind unbekannt -> nil -> Prelude-Fehler.
// ---------------------------------------------------------------------------
namespace {

void DbSetInt(mrb_state* mrb, mrb_value h, const char* k, mrb_int v) {
    // mruby 4.0: mrb_intern ist 3-argumentig (Laenge mitgeben)
    mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern(mrb, k, strlen(k))),
                 RPG_MRB_INT_VALUE(mrb, v));
}
void DbSetStr(mrb_state* mrb, mrb_value h, const char* k, const std::string& s) {
    mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern(mrb, k, strlen(k))),
                 mrb_str_new(mrb, s.data(), (mrb_int)s.size()));
}
void DbSetBool(mrb_state* mrb, mrb_value h, const char* k, bool v) {
    mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern(mrb, k, strlen(k))),
                 v ? mrb_true_value() : mrb_false_value());
}
mrb_value DbIntArray(mrb_state* mrb, const std::vector<int>& v) {
    mrb_value a = mrb_ary_new_capa(mrb, (mrb_int)v.size());
    for (int x : v) mrb_ary_push(mrb, a, RPG_MRB_INT_VALUE(mrb, x));
    return a;
}
mrb_value DbStrArray(mrb_state* mrb, const std::vector<std::string>& v) {
    mrb_value a = mrb_ary_new_capa(mrb, (mrb_int)v.size());
    for (const auto& s : v)
        mrb_ary_push(mrb, a, mrb_str_new(mrb, s.data(), (mrb_int)s.size()));
    return a;
}

} // anonymous namespace

static mrb_value rb_engine_db_fetch(mrb_state* mrb, mrb_value /*self*/) {
    char* kindC = nullptr;
    mrb_int mapId = 1; // nur fuer kind == "map" (zweites, optionales Arg)
    mrb_get_args(mrb, "z|i", &kindC, &mapId);
    const std::string kind = kindC ? kindC : "";
    Database& db = Database::Get();

    if (kind == "map") {
        // XP Stufe 2 (Map%03d.rxdata): Engine-Map-Datei (Binaerformat,
        // Map::Load) laden und als generischen Hash liefern — Ruby baut
        // daraus RPG::Map (data-Table mit +384-RGSS-Offset). tileset/encounter
        // kommen aus MapInfo (die .map-Datei fuehrt sie nicht). Events
        // bleiben bewusst leer: NPC-Darstellung + Interpreter laufen nativ
        // ueber das EventSystem (LoadMapEvents haette hier Singleton-
        // Seiteneffekte, deshalb kein Export an dieser Stelle).
        const std::string base = RgssUI::ProjectBase().empty()
            ? std::string(".") : RgssUI::ProjectBase();
        char rel[64];
        std::snprintf(rel, sizeof(rel), "maps/map%d.map", (int)mapId);
        const std::string p1 = base + "/" + rel;
        Map m;
        const bool ok = std::filesystem::exists(p1) ? m.Load(p1) : m.Load(rel);
        if (!ok) return mrb_nil_value();

        int tilesetId = 1;
        int encStep = 30;
        std::vector<int> encList;
        for (const auto& mi : db.MapInfos()) {
            if (mi.id == (int)mapId) {
                tilesetId = mi.tilesetId;
                encStep = mi.encounterStep;
                for (int k = 0; k < 8; ++k)
                    if (mi.encounterList[k] > 0) encList.push_back(mi.encounterList[k]);
                break;
            }
        }

        mrb_value h = mrb_hash_new(mrb);
        DbSetInt(mrb, h, "width", m.GetWidth());
        DbSetInt(mrb, h, "height", m.GetHeight());
        DbSetInt(mrb, h, "tileset_id", tilesetId);
        DbSetInt(mrb, h, "encounter_step", encStep);
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "encounter_list")),
                     DbIntArray(mrb, encList));
        // Layer z-major roh (-1 = leer); Ruby verschiebt native ID -> +384.
        const auto& layers = m.GetLayers();
        mrb_value lays = mrb_ary_new_capa(mrb, (mrb_int)layers.size());
        for (const auto& lay : layers) {
            mrb_ary_push(mrb, lays, DbIntArray(mrb, lay.tiles));
        }
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "layers")), lays);
        return h;
    }
    if (kind == "actors") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Actors().size());
        for (const auto& a : db.Actors()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", a.id);
            DbSetStr(mrb, h, "name", a.name);
            // Klassen-ID ueber den Klassennamen suchen (unsere DB referenziert
            // per Name, XP per ID) — unbekannt -> 1 (Erste Klasse).
            int classId = 1;
            for (const auto& c : db.Classes())
                if (c.name == a.className) { classId = c.id; break; }
            DbSetInt(mrb, h, "class_id", classId);
            DbSetInt(mrb, h, "initial_level", a.initialLevel);
            DbSetInt(mrb, h, "final_level", a.maxLevel);
            DbSetStr(mrb, h, "character_name", a.characterName);
            DbSetStr(mrb, h, "battler_name", a.battlerName);
            // Parameter-Endwerte: Ruby interpoliert initial->final ueber 99 Level
            DbSetInt(mrb, h, "init_mhp", a.initialStats.mhp);
            DbSetInt(mrb, h, "init_mmp", a.initialStats.mmp);
            DbSetInt(mrb, h, "init_atk", a.initialStats.atk);
            DbSetInt(mrb, h, "init_def", a.initialStats.def);
            DbSetInt(mrb, h, "init_mat", a.initialStats.mat);
            DbSetInt(mrb, h, "init_agi", a.initialStats.agi);
            DbSetInt(mrb, h, "fin_mhp", a.finalStats.mhp);
            DbSetInt(mrb, h, "fin_mmp", a.finalStats.mmp);
            DbSetInt(mrb, h, "fin_atk", a.finalStats.atk);
            DbSetInt(mrb, h, "fin_def", a.finalStats.def);
            DbSetInt(mrb, h, "fin_mat", a.finalStats.mat);
            DbSetInt(mrb, h, "fin_agi", a.finalStats.agi);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "classes") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Classes().size());
        for (const auto& c : db.Classes()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", c.id);
            DbSetStr(mrb, h, "name", c.name);
            mrb_value ls = mrb_ary_new_capa(mrb, (mrb_int)c.learnings.size());
            for (const auto& l : c.learnings) {
                mrb_value pair = mrb_ary_new_capa(mrb, 2);
                mrb_ary_push(mrb, pair, RPG_MRB_INT_VALUE(mrb, l.level));
                mrb_ary_push(mrb, pair, RPG_MRB_INT_VALUE(mrb, l.skillId));
                mrb_ary_push(mrb, ls, pair);
            }
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "learnings")), ls);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "skills") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Skills().size());
        for (const auto& s : db.Skills()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", s.id);
            DbSetStr(mrb, h, "name", s.name);
            DbSetStr(mrb, h, "description", s.description);
            DbSetInt(mrb, h, "scope", s.scope);
            DbSetInt(mrb, h, "sp_cost", s.mpCost);
            DbSetInt(mrb, h, "power", s.power);
            // animation ist bei uns ein NAME; XP nutzt animation1_id
            // -> ueber die System-Animationsliste zurueckindizieren (Index+1).
            int animId = 0;
            for (size_t ai = 0; ai < db.System().animations.size(); ++ai)
                if (db.System().animations[ai] == s.animation) { animId = (int)ai + 1; break; }
            DbSetInt(mrb, h, "animation1_id", animId);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "items") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Items().size());
        for (const auto& it : db.Items()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", it.id);
            DbSetStr(mrb, h, "name", it.name);
            DbSetStr(mrb, h, "description", it.description);
            DbSetInt(mrb, h, "price", it.price);
            DbSetBool(mrb, h, "consumable", it.consumable);
            DbSetInt(mrb, h, "scope", (mrb_int)it.scope); // Enum-Reihenfolge = XP
            DbSetInt(mrb, h, "recover_hp", it.hpRecovery);
            DbSetInt(mrb, h, "recover_sp", it.mpRecovery);
            DbSetInt(mrb, h, "animation1_id", it.animationId);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "weapons") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Weapons().size());
        for (const auto& w : db.Weapons()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", w.id);
            DbSetStr(mrb, h, "name", w.name);
            DbSetStr(mrb, h, "description", w.description);
            DbSetInt(mrb, h, "price", w.price);
            DbSetInt(mrb, h, "atk", w.atk);
            DbSetInt(mrb, h, "animation1_id", w.animationId);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "armors") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Armors().size());
        for (const auto& ar : db.Armors()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", ar.id);
            DbSetStr(mrb, h, "name", ar.name);
            DbSetStr(mrb, h, "description", ar.description);
            DbSetInt(mrb, h, "price", ar.price);
            DbSetInt(mrb, h, "pdef", ar.def);
            DbSetInt(mrb, h, "mdef", ar.mdf);
            DbSetInt(mrb, h, "kind", (mrb_int)ar.armorType); // 0..3 wie XP
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "enemies") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Enemies().size());
        for (const auto& e : db.Enemies()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", e.id);
            DbSetStr(mrb, h, "name", e.name);
            DbSetStr(mrb, h, "battler_name", e.battlerName);
            DbSetInt(mrb, h, "battler_hue", e.battlerHue);
            DbSetInt(mrb, h, "maxhp", e.maxHp);
            DbSetInt(mrb, h, "maxsp", e.maxMp);
            // XP-Gegner: str/dex/agi/int (Kurven) + atk/pdef/mdef absolut.
            // Unsere Stats mappen: atk->str/atk, def->dex/pdef, mdf->mdef,
            // mat->int. luck hat XP an Gegnern nicht (bleibt Engine-intern).
            DbSetInt(mrb, h, "str", e.atk);
            DbSetInt(mrb, h, "dex", e.def);
            DbSetInt(mrb, h, "agi", e.agi);
            DbSetInt(mrb, h, "int", e.mat);
            DbSetInt(mrb, h, "atk", e.atk);
            DbSetInt(mrb, h, "pdef", e.def);
            DbSetInt(mrb, h, "mdef", e.mdf);
            DbSetInt(mrb, h, "exp", e.exp);
            DbSetInt(mrb, h, "gold", e.gold);
            DbSetInt(mrb, h, "item_id", e.dropItems.empty() ? 0 : e.dropItems[0]);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "troops") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Troops().size());
        for (const auto& t : db.Troops()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", t.id);
            DbSetStr(mrb, h, "name", t.name);
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "members")),
                         DbIntArray(mrb, t.members));
            mrb_value pages = mrb_ary_new_capa(mrb, (mrb_int)t.pages.size());
            for (const auto& p : t.pages) {
                mrb_value ph = mrb_hash_new(mrb);
                DbSetBool(mrb, ph, "turn_valid", p.turnValid);
                DbSetInt(mrb, ph, "turn_a", p.turnA);
                DbSetInt(mrb, ph, "turn_b", p.turnB);
                DbSetBool(mrb, ph, "enemy_valid", p.enemyValid);
                DbSetInt(mrb, ph, "enemy_index", p.enemyIndex);
                DbSetInt(mrb, ph, "enemy_hp", p.enemyHpBelow);
                DbSetBool(mrb, ph, "actor_valid", p.actorValid);
                DbSetInt(mrb, ph, "actor_id", p.actorIndex);
                DbSetInt(mrb, ph, "actor_hp", p.actorHpBelow);
                DbSetBool(mrb, ph, "switch_valid", p.switchValid);
                DbSetInt(mrb, ph, "switch_id", p.switchId);
                DbSetInt(mrb, ph, "span", p.span);
                DbSetInt(mrb, ph, "common_event_id", p.commonEventId);
                mrb_ary_push(mrb, pages, ph);
            }
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "pages")), pages);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "states") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.States().size());
        for (const auto& s : db.States()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", s.id);
            DbSetStr(mrb, h, "name", s.name);
            DbSetInt(mrb, h, "restriction", s.restriction);
            DbSetInt(mrb, h, "rating", s.priority); // XP rating = unsere Prioritaet
            DbSetBool(mrb, h, "slip_damage", s.hpDrainRate > 0.0f);
            DbSetBool(mrb, h, "battle_only", s.removeAtBattleEnd);
            DbSetInt(mrb, h, "hold_turn", s.holdTurn);
            DbSetInt(mrb, h, "auto_release_prob", s.autoRemovalTiming != 0 ? 100 : 0);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "animations") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.AnimationSet().size());
        for (const auto& an : db.AnimationSet()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", an.id);
            DbSetStr(mrb, h, "name", an.name);
            DbSetStr(mrb, h, "animation_name", an.file);
            DbSetInt(mrb, h, "position", an.position);
            DbSetInt(mrb, h, "frame_max", an.frames.empty() ? 1 : (mrb_int)an.frames.size());
            mrb_value frames = mrb_ary_new_capa(mrb, (mrb_int)an.frames.size());
            for (const auto& f : an.frames) {
                mrb_value fh = mrb_hash_new(mrb);
                mrb_value cells = mrb_ary_new_capa(mrb, (mrb_int)f.cells.size());
                for (const auto& c : f.cells) {
                    mrb_value cell = mrb_ary_new_capa(mrb, 6);
                    mrb_ary_push(mrb, cell, RPG_MRB_INT_VALUE(mrb, c.cellId));
                    mrb_ary_push(mrb, cell, RPG_MRB_INT_VALUE(mrb, c.x));
                    mrb_ary_push(mrb, cell, RPG_MRB_INT_VALUE(mrb, c.y));
                    mrb_ary_push(mrb, cell, RPG_MRB_INT_VALUE(mrb, c.scale));
                    mrb_ary_push(mrb, cell, RPG_MRB_INT_VALUE(mrb, c.rotation));
                    mrb_ary_push(mrb, cell, RPG_MRB_INT_VALUE(mrb, c.opacity));
                    mrb_ary_push(mrb, cells, cell);
                }
                mrb_hash_set(mrb, fh, mrb_symbol_value(mrb_intern_lit(mrb, "cells")), cells);
                DbSetStr(mrb, fh, "se_name", f.seName);
                DbSetInt(mrb, fh, "se_volume", f.seVolume);
                DbSetInt(mrb, fh, "se_pitch", f.sePitch);
                DbSetInt(mrb, fh, "flash_scope", f.flashScope);
                DbSetInt(mrb, fh, "flash_r", f.flashR);
                DbSetInt(mrb, fh, "flash_g", f.flashG);
                DbSetInt(mrb, fh, "flash_b", f.flashB);
                DbSetInt(mrb, fh, "flash_duration", f.flashDuration);
                mrb_ary_push(mrb, frames, fh);
            }
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "frames")), frames);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "tilesets") {
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)db.Tilesets().size());
        for (const auto& ts : db.Tilesets()) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", ts.id);
            DbSetStr(mrb, h, "name", ts.name);
            DbSetStr(mrb, h, "tileset_name", ts.tilesetName);
            mrb_value autos = mrb_ary_new_capa(mrb, 7);
            for (int i = 0; i < 7; ++i)
                mrb_ary_push(mrb, autos,
                             mrb_str_new(mrb, ts.autotileNames[i].data(),
                                         (mrb_int)ts.autotileNames[i].size()));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "autotile_names")), autos);
            DbSetStr(mrb, h, "panorama_name", ts.panoramaName);
            DbSetStr(mrb, h, "fog_name", ts.fogName);
            DbSetStr(mrb, h, "battleback_name", ts.battlebackName);
            // Rohtabellen ohne den RGSS-Autotile-Offset (0..383) — die
            // Ruby-Seite baut daraus Tables mit XP-Offset 384.
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "passages")),
                         DbIntArray(mrb, ts.flags));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "priorities")),
                         DbIntArray(mrb, ts.priority));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "terrain_tags")),
                         DbIntArray(mrb, ts.terrainTags));
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "system") {
        const SystemData& sys = db.System();
        mrb_value h = mrb_hash_new(mrb);
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "party_members")),
                     DbIntArray(mrb, sys.initialParty));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "elements")),
                     DbStrArray(mrb, sys.elements));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "switches")),
                     DbStrArray(mrb, sys.switches));
        mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "variables")),
                     DbStrArray(mrb, sys.variables));
        DbSetStr(mrb, h, "windowskin_name", sys.windowskinName);
        DbSetStr(mrb, h, "title_name", sys.titleGraphicName);
        DbSetStr(mrb, h, "gameover_name", sys.gameoverGraphicName);
        DbSetStr(mrb, h, "battle_transition", sys.battleTransitionName);
        DbSetStr(mrb, h, "title_bgm", sys.titleBgm);
        DbSetStr(mrb, h, "battle_bgm", sys.battleBgm);
        DbSetStr(mrb, h, "battle_end_me", sys.battleEndMe);
        DbSetStr(mrb, h, "gameover_me", sys.gameoverMe);
        DbSetStr(mrb, h, "cursor_se", sys.cursorSe);
        DbSetStr(mrb, h, "decision_se", sys.decisionSe);
        DbSetStr(mrb, h, "cancel_se", sys.cancelSe);
        DbSetStr(mrb, h, "buzzer_se", sys.buzzerSe);
        DbSetStr(mrb, h, "equip_se", sys.equipSe);
        DbSetStr(mrb, h, "shop_se", sys.shopSe);
        DbSetStr(mrb, h, "save_se", sys.saveSe);
        DbSetStr(mrb, h, "load_se", sys.loadSe);
        DbSetStr(mrb, h, "battle_start_se", sys.battleStartSe);
        DbSetStr(mrb, h, "escape_se", sys.escapeSe);
        DbSetStr(mrb, h, "actor_collapse_se", sys.actorCollapseSe);
        DbSetStr(mrb, h, "enemy_collapse_se", sys.enemyCollapseSe);
        DbSetInt(mrb, h, "start_map_id", sys.startMapId);
        DbSetInt(mrb, h, "start_x", sys.startX);
        DbSetInt(mrb, h, "start_y", sys.startY);
        // word_* als flache Liste im XP-Words-Reihenfolgeprofil
        DbSetStr(mrb, h, "word_gold", sys.currencyUnit);
        DbSetStr(mrb, h, "word_hp", sys.wordHp);
        DbSetStr(mrb, h, "word_sp", sys.wordSp);
        DbSetStr(mrb, h, "word_str", sys.wordStr);
        DbSetStr(mrb, h, "word_dex", sys.wordDex);
        DbSetStr(mrb, h, "word_agi", sys.wordAgi);
        DbSetStr(mrb, h, "word_int", sys.wordInt);
        DbSetStr(mrb, h, "word_atk", sys.wordAtk);
        DbSetStr(mrb, h, "word_pdef", sys.wordPdef);
        DbSetStr(mrb, h, "word_mdef", sys.wordMdef);
        DbSetStr(mrb, h, "word_skill", sys.wordSkill);
        DbSetStr(mrb, h, "word_item", sys.wordItem);
        DbSetStr(mrb, h, "word_weapon", sys.wordWeapon);
        DbSetStr(mrb, h, "word_armor1", sys.wordShield);
        DbSetStr(mrb, h, "word_armor2", sys.wordHelmet);
        DbSetStr(mrb, h, "word_armor3", sys.wordBodyArmor);
        DbSetStr(mrb, h, "word_armor4", sys.wordAccessory);
        DbSetStr(mrb, h, "word_attack", sys.wordAttack);
        DbSetStr(mrb, h, "word_guard", sys.wordDefend);
        return h; // einzelnes Objekt, kein Array
    }
    if (kind == "common_events") {
        // Stufe 3 (CommonEvents.rxdata): Die gemeinsamen Ereignisse liegen
        // im EventSystem (Tab „Gem. Events" der Datenbank). Unsere
        // EventCommandCode-Enum ist exakt XP-kodiert (101..355), daher
        // 1:1-Export; param1..3/text/parameters roh — der Prelude kuerzt
        // nur Trailing-Defaults (XP-Daten enden nicht auf Defaults).
        const auto& cevs = EventSystem::Get().GetCommonEvents();
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)cevs.size());
        for (const auto& ce : cevs) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", ce.id);
            DbSetStr(mrb, h, "name", ce.name);
            // XP trigger: 0 = keiner, 1 = Autorun, 2 = Parallelprozess
            int trig = 0;
            if (ce.trigger == EventTrigger::Autorun) trig = 1;
            else if (ce.trigger == EventTrigger::Parallel) trig = 2;
            DbSetInt(mrb, h, "trigger", trig);
            DbSetInt(mrb, h, "switch_id", ce.switchId);
            mrb_value list = mrb_ary_new_capa(mrb, (mrb_int)ce.list.size());
            for (const auto& cmd : ce.list) {
                mrb_value ch = mrb_hash_new(mrb);
                DbSetInt(mrb, ch, "code", (mrb_int)cmd.code);
                DbSetInt(mrb, ch, "indent", cmd.indent);
                DbSetInt(mrb, ch, "p1", cmd.param1);
                DbSetInt(mrb, ch, "p2", cmd.param2);
                DbSetInt(mrb, ch, "p3", cmd.param3);
                DbSetStr(mrb, ch, "text", cmd.text);
                mrb_value ps = mrb_ary_new_capa(mrb, (mrb_int)cmd.parameters.size());
                for (const auto& pstr : cmd.parameters) {
                    // Zahlen als Integer liefern (XP-Parameter sind gemischt
                    // int/string), alles andere als String. Nur vollstaendig
                    // geparste, nicht-leere Strings gelten als Zahl.
                    if (!pstr.empty()) {
                        char* endP = nullptr;
                        const long v = std::strtol(pstr.c_str(), &endP, 10);
                        if (endP && *endP == '\0') {
                            mrb_ary_push(mrb, ps, RPG_MRB_INT_VALUE(mrb, v));
                            continue;
                        }
                    }
                    mrb_ary_push(mrb, ps,
                                 mrb_str_new(mrb, pstr.data(), (mrb_int)pstr.size()));
                }
                mrb_hash_set(mrb, ch, mrb_symbol_value(mrb_intern_lit(mrb, "params")), ps);
                mrb_ary_push(mrb, list, ch);
            }
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "list")), list);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    if (kind == "mapinfos") {
        const auto& infos = db.MapInfos();
        mrb_value out = mrb_ary_new_capa(mrb, (mrb_int)infos.size());
        for (const auto& mi : infos) {
            mrb_value h = mrb_hash_new(mrb);
            DbSetInt(mrb, h, "id", mi.id);
            DbSetStr(mrb, h, "name", mi.name);
            DbSetInt(mrb, h, "parent_id", mi.parentId);
            DbSetInt(mrb, h, "order", mi.order);
            DbSetBool(mrb, h, "expanded", mi.expanded);
            DbSetInt(mrb, h, "scroll_x", mi.scrollX);
            DbSetInt(mrb, h, "scroll_y", mi.scrollY);
            mrb_ary_push(mrb, out, h);
        }
        return out;
    }
    return mrb_nil_value(); // unbekannt/nicht verdrahtet -> Prelude meldet das
}

void RubyVM::BindRgssGraphics() {
    // Graphics-Modul
    struct RClass* gfx = mrb_define_module(mMrb, "Graphics");
    mrb_define_module_function(mMrb, gfx, "update", rb_graphics_update, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gfx, "freeze", rb_graphics_freeze, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gfx, "transition", rb_graphics_transition, MRB_ARGS_ANY());
    mrb_define_module_function(mMrb, gfx, "frame_reset", rb_graphics_frame_reset, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gfx, "frame_rate", rb_graphics_frame_rate_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gfx, "frame_rate=", rb_graphics_frame_rate_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gfx, "frame_count", rb_graphics_frame_count_get, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gfx, "frame_count=", rb_graphics_frame_count_set, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, gfx, "width", rb_graphics_width, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, gfx, "height", rb_graphics_height, MRB_ARGS_NONE());

    // Input (XP-Satz zusätzlich zu key_down?/key_pressed?)
    struct RClass* input = mrb_module_get(mMrb, "Input");
    mrb_define_const(mMrb, input, "DOWN", RPG_MRB_INT_VALUE(mMrb, 2));
    mrb_define_const(mMrb, input, "LEFT", RPG_MRB_INT_VALUE(mMrb, 4));
    mrb_define_const(mMrb, input, "RIGHT", RPG_MRB_INT_VALUE(mMrb, 6));
    mrb_define_const(mMrb, input, "UP", RPG_MRB_INT_VALUE(mMrb, 8));
    mrb_define_const(mMrb, input, "A", RPG_MRB_INT_VALUE(mMrb, 11));
    mrb_define_const(mMrb, input, "B", RPG_MRB_INT_VALUE(mMrb, 12));
    mrb_define_const(mMrb, input, "C", RPG_MRB_INT_VALUE(mMrb, 13));
    mrb_define_const(mMrb, input, "X", RPG_MRB_INT_VALUE(mMrb, 14));
    mrb_define_const(mMrb, input, "Y", RPG_MRB_INT_VALUE(mMrb, 15));
    mrb_define_const(mMrb, input, "Z", RPG_MRB_INT_VALUE(mMrb, 16));
    mrb_define_const(mMrb, input, "L", RPG_MRB_INT_VALUE(mMrb, 17));
    mrb_define_const(mMrb, input, "R", RPG_MRB_INT_VALUE(mMrb, 18));
    mrb_define_const(mMrb, input, "SHIFT", RPG_MRB_INT_VALUE(mMrb, 21));
    mrb_define_const(mMrb, input, "CTRL", RPG_MRB_INT_VALUE(mMrb, 22));
    mrb_define_const(mMrb, input, "ALT", RPG_MRB_INT_VALUE(mMrb, 23));
    mrb_define_const(mMrb, input, "F5", RPG_MRB_INT_VALUE(mMrb, 25));
    mrb_define_const(mMrb, input, "F6", RPG_MRB_INT_VALUE(mMrb, 26));
    mrb_define_const(mMrb, input, "F7", RPG_MRB_INT_VALUE(mMrb, 27));
    mrb_define_const(mMrb, input, "F8", RPG_MRB_INT_VALUE(mMrb, 28));
    mrb_define_const(mMrb, input, "F9", RPG_MRB_INT_VALUE(mMrb, 29));
    mrb_define_module_function(mMrb, input, "update", rb_input_update, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, input, "press?", rb_input_press_p, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, input, "trigger?", rb_input_trigger_p, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, input, "repeat?", rb_input_repeat_p, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, input, "dir4", rb_input_dir4, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, input, "dir8", rb_input_dir8, MRB_ARGS_NONE());

    // Audio (XP-Signaturen - setzt die alten play-Bindings ab)
    struct RClass* audio = mrb_module_get(mMrb, "Audio");
    mrb_define_module_function(mMrb, audio, "bgm_play", rb_audio_bgm_play_xp, MRB_ARGS_ANY());
    mrb_define_module_function(mMrb, audio, "bgs_play", rb_audio_bgs_play_xp, MRB_ARGS_ANY());
    mrb_define_module_function(mMrb, audio, "me_play", rb_audio_me_play_xp, MRB_ARGS_ANY());
    mrb_define_module_function(mMrb, audio, "se_play", rb_audio_se_play_xp, MRB_ARGS_ANY());
    mrb_define_module_function(mMrb, audio, "bgm_stop", rb_audio_bgm_stop_xp, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, audio, "bgs_stop", rb_audio_bgs_stop_xp, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, audio, "me_stop", rb_audio_me_stop_xp, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, audio, "se_stop", rb_audio_se_stop_xp, MRB_ARGS_NONE());
    mrb_define_module_function(mMrb, audio, "bgm_fade", rb_audio_bgm_fade_xp, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, audio, "bgs_fade", rb_audio_bgs_fade_xp, MRB_ARGS_REQ(1));
    mrb_define_module_function(mMrb, audio, "me_fade", rb_audio_me_fade_xp, MRB_ARGS_REQ(1));

    // XP load_data-Bruecke (PAKET 6): Engine-DB (JSON) -> Ruby-Hashes.
    // Privater Kernel-Helfer; der Prelude baut daraus die RPG::*-Objekte.
    // kind "map" nimmt zusaetzlich die Map-ID als zweites Argument.
    mrb_define_method(mMrb, mMrb->kernel_module, "__engine_db_fetch",
                      rb_engine_db_fetch, MRB_ARGS_ARG(1, 1));
}

void RubyVM::BindRgssWindowEx() {
    // Klasse "Window" existiert bereits (RGSS-Basis aus RubyVM.cpp) -
    // Vollset nach XP: windowskin als Bitmap, contents, cursor_rect,
    // active/pause/stretch, opacities, ox/oy, viewport, XP-initialize.
    struct RClass* win = mrb_class_get(mMrb, "Window");
    mrb_define_method(mMrb, win, "initialize", rb_win_init_xp, MRB_ARGS_ANY());
    mrb_define_method(mMrb, win, "viewport", rb_win_viewport_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "windowskin", rb_win_windowskin_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "windowskin=", rb_win_windowskin_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "contents", rb_win_contents_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "contents=", rb_win_contents_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "cursor_rect", rb_win_cursor_rect_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "cursor_rect=", rb_win_cursor_rect_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "active", rb_win_active_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "active=", rb_win_active_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "pause", rb_win_pause_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "pause=", rb_win_pause_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "stretch", rb_win_stretch_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "stretch=", rb_win_stretch_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "opacity", rb_win_opacity_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "opacity=", rb_win_opacity_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "back_opacity", rb_win_back_opacity_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "back_opacity=", rb_win_back_opacity_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "contents_opacity", rb_win_contents_opacity_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "contents_opacity=", rb_win_contents_opacity_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "ox", rb_win_ox_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "ox=", rb_win_ox_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "oy", rb_win_oy_get, MRB_ARGS_NONE());
    mrb_define_method(mMrb, win, "oy=", rb_win_oy_set, MRB_ARGS_REQ(1));
    mrb_define_method(mMrb, win, "update", rb_win_update_xp, MRB_ARGS_NONE());
}

void RubyVM::LoadRgssPrelude() {
    ExecuteString(kRgssPrelude, "<rgss-prelude>");
}

} // namespace rpg

#else // !RPGMAKER3D_ENABLE_RUBY

namespace rpg {
void RubyVM::BindRgssObjects() {}
void RubyVM::BindRgssDrawables() {}
void RubyVM::BindRgssGraphics() {}
void RubyVM::BindRgssWindowEx() {}
void RubyVM::LoadRgssPrelude() {}
} // namespace rpg

#endif // RPGMAKER3D_ENABLE_RUBY
