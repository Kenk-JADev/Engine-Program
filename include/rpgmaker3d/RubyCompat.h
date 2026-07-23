#pragma once
// ============================================================================
// mruby-Versionskompatibilitaet (nur im RPGMAKER3D_ENABLE_RUBY-Zweig nutzen!)
// ----------------------------------------------------------------------------
// mruby 2.x        : mrb_fixnum_value(i)            (kein mrb_state-Parameter)
// mruby 3.x        : mrb_integer_value(mrb, i)
// mruby 4.x        : mrb_int_value(mrb, i)          (mrb_integer_value entfernt)
// Der CI-Build verwendet mruby 4.0.0.
//
// Benutzung im eigenen Code:
//   RPG_MRB_INT_VALUE(v)        -> mrb_state aus dem Variablennamen "mrb"
//   RPG_MRB_INT_VALUE(mrb, v)   -> expliziter State
// ============================================================================

#include <mruby.h>
#include <mruby/value.h>
#include <mruby/version.h>

// 1/2-Argument-Umschaltung
#define RPG_MRB_IVA(_1, _2, NAME, ...) NAME
#define RPG_MRB_INT_VALUE(...) \
    RPG_MRB_IVA(__VA_ARGS__, RPG_MRB_INT_VALUE_2, RPG_MRB_INT_VALUE_1)(__VA_ARGS__)

#if defined(MRUBY_RELEASE_MAJOR) && (MRUBY_RELEASE_MAJOR >= 4)
    #define RPG_MRB_INT_VALUE_1(i)      mrb_int_value(mrb, (mrb_int)(i))
    #define RPG_MRB_INT_VALUE_2(mrb_, i) mrb_int_value((mrb_), (mrb_int)(i))
#elif defined(MRUBY_RELEASE_MAJOR) && (MRUBY_RELEASE_MAJOR >= 3)
    #define RPG_MRB_INT_VALUE_1(i)      mrb_integer_value(mrb, (mrb_int)(i))
    #define RPG_MRB_INT_VALUE_2(mrb_, i) mrb_integer_value((mrb_), (mrb_int)(i))
#else
    #define RPG_MRB_INT_VALUE_1(i)      ( (void)mrb, mrb_fixnum_value((mrb_int)(i)) )
    #define RPG_MRB_INT_VALUE_2(mrb_, i) ( (void)(mrb_), mrb_fixnum_value((mrb_int)(i)) )
#endif
