#pragma once
// ============================================================================
// mruby-Versionskompatibilitaet (nur im RPGMAKER3D_ENABLE_RUBY-Zweig nutzen!)
// ----------------------------------------------------------------------------
// mruby 2.x        : mrb_fixnum_value(i)            (kein mrb_state-Parameter)
// mruby 3.x        : mrb_integer_value(mrb, i)
// mruby 4.x        : mrb_int_value(mrb, i)          (mrb_integer_value entfernt)
// Der CI-Build verwendet mruby 4.0.0.
//
// Benutzung im eigenen Code -- IMMER mit ZWEI Argumenten:
//   RPG_MRB_INT_VALUE(mrb, v)    in Funktionen mit mrb_state* mrb
//   RPG_MRB_INT_VALUE(mMrb, v)   im Install-Scope (State heisst dort mMrb)
// HINWEIS: Keine __VA_ARGS__-1/2-Arg-Umschaltung mehr -- der MSVC-Legacy-
// Praeprozessor loest die nicht zuverlaessig auf (CI-Fehler C2065).
// ============================================================================

#include <mruby.h>
#include <mruby/value.h>
#include <mruby/version.h>

#if defined(MRUBY_RELEASE_MAJOR) && (MRUBY_RELEASE_MAJOR >= 4)
    #define RPG_MRB_INT_VALUE(mrb_, i) mrb_int_value((mrb_), (mrb_int)(i))
#elif defined(MRUBY_RELEASE_MAJOR) && (MRUBY_RELEASE_MAJOR >= 3)
    #define RPG_MRB_INT_VALUE(mrb_, i) mrb_integer_value((mrb_), (mrb_int)(i))
#else
    #define RPG_MRB_INT_VALUE(mrb_, i) ( (void)(mrb_), mrb_fixnum_value((mrb_int)(i)) )
#endif
