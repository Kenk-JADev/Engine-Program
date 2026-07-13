#include "rpgmaker3d/EditorStyle.h"
#include "rpgmaker3d/Logger.h"
#include <imgui.h>
#include <algorithm>

namespace rpg {

// ==================== Static Member Definitions ====================
EditorColors EditorStyle::s_DarkColors;
EditorColors EditorStyle::s_LightColors;
EditorColors EditorStyle::s_ClassicColors;
EditorColors EditorStyle::s_CurrentColors;
EditorTheme  EditorStyle::s_CurrentTheme = EditorTheme::Dark;
float        EditorStyle::s_FontScale = 1.0f;

ImFont* EditorStyle::s_MainFont = nullptr;
ImFont* EditorStyle::s_IconFont = nullptr;
ImFont* EditorStyle::s_MonoFont = nullptr;
ImFont* EditorStyle::s_LargeFont = nullptr;
ImFont* EditorStyle::s_SmallFont = nullptr;

// ==================== FontAwesome Icons (FontAwesome 6 Free Solid) ====================
// Defined in .cpp to avoid MSVC C4566 warnings with universal character names in headers
namespace Icons {
    // File/Folder operations
    inline constexpr const char* FOLDER      = "\uf07b";
    inline constexpr const char* FOLDER_OPEN = "\uf07c";
    inline constexpr const char* FILE        = "\uf15b";
    inline constexpr const char* FILE_ALT    = "\uf15c";
    inline constexpr const char* FILE_CODE   = "\uf1c9";

    // Media
    inline constexpr const char* PLAY        = "\uf04b";
    inline constexpr const char* PAUSE       = "\uf04c";
    inline constexpr const char* STOP        = "\uf04d";
    inline constexpr const char* MUSIC       = "\uf001";
    inline constexpr const char* VOLUME_UP   = "\uf028";
    inline constexpr const char* VOLUME_MUTE = "\uf6a9";
    inline constexpr const char* IMAGE       = "\uf03e";
    inline constexpr const char* VIDEO       = "\uf03d";
    inline constexpr const char* CAMERA      = "\uf030";
    inline constexpr const char* FILM        = "\uf008";

    // Editing
    inline constexpr const char* SAVE        = "\uf0c7";
    inline constexpr const char* UNDO        = "\uf0e2";
    inline constexpr const char* REDO        = "\uf01e";
    inline constexpr const char* PLUS        = "\uf067";
    inline constexpr const char* MINUS       = "\uf068";
    inline constexpr const char* TRASH       = "\uf1f8";
    inline constexpr const char* EDIT        = "\uf044";
    inline constexpr const char* COPY        = "\uf0c5";
    inline constexpr const char* CUT         = "\uf0c4";
    inline constexpr const char* PASTE       = "\uf0ea";

    // View
    inline constexpr const char* EYE         = "\uf06e";
    inline constexpr const char* EYE_SLASH   = "\uf070";
    inline constexpr const char* SEARCH      = "\uf002";
    inline constexpr const char* ZOOM_IN     = "\uf00e";
    inline constexpr const char* ZOOM_OUT    = "\uf010";
    inline constexpr const char* EXPAND      = "\uf065";
    inline constexpr const char* COMPRESS    = "\uf066";
    inline constexpr const char* ARROWS_ALT  = "\uf0b2";

    // UI State
    inline constexpr const char* LOCK        = "\uf023";
    inline constexpr const char* UNLOCK      = "\uf09c";
    inline constexpr const char* CHECK       = "\uf00c";
    inline constexpr const char* TIMES       = "\uf00d";
    inline constexpr const char* COG         = "\uf013";
    inline constexpr const char* HOME        = "\uf015";
    inline constexpr const char* BARS        = "\uf0c9";

    // Warnings/Info
    inline constexpr const char* EXCLAMATION_TRIANGLE = "\uf071";
    inline constexpr const char* INFO_CIRCLE          = "\uf05a";
    inline constexpr const char* QUESTION_CIRCLE      = "\uf059";
    inline constexpr const char* EXCLAMATION_CIRCLE   = "\uf06a";
    inline constexpr const char* BAN                  = "\uf05e";
    inline constexpr const char* SPINNER              = "\uf110";

    // Navigation
    inline constexpr const char* CHEVRON_UP    = "\uf077";
    inline constexpr const char* CHEVRON_DOWN  = "\uf078";
    inline constexpr const char* CHEVRON_LEFT  = "\uf053";
    inline constexpr const char* CHEVRON_RIGHT = "\uf054";
    inline constexpr const char* ARROW_UP      = "\uf062";
    inline constexpr const char* ARROW_DOWN    = "\uf063";
    inline constexpr const char* ARROW_LEFT    = "\uf060";
    inline constexpr const char* ARROW_RIGHT   = "\uf061";
    inline constexpr const char* ANGLE_UP      = "\uf106";
    inline constexpr const char* ANGLE_DOWN    = "\uf107";
    inline constexpr const char* ANGLE_LEFT    = "\uf104";
    inline constexpr const char* ANGLE_RIGHT   = "\uf105";
    inline constexpr const char* CARET_UP      = "\uf0d8";
    inline constexpr const char* CARET_DOWN    = "\uf0d7";
    inline constexpr const char* CARET_LEFT    = "\uf0d9";
    inline constexpr const char* CARET_RIGHT   = "\uf0da";

    // Game/Scene specific
    inline constexpr const char* CUBE            = "\uf1b2";
    inline constexpr const char* PUZZLE_PIECE    = "\uf12e";
    inline constexpr const char* MAGIC           = "\uf0d0";
    inline constexpr const char* LIGHTBULB       = "\uf0eb";
    inline constexpr const char* PAINT_BRUSH     = "\uf1fc";
    inline constexpr const char* LAYER_GROUP     = "\uf5fd";
    inline constexpr const char* OBJECT_GROUP    = "\uf247";
    inline constexpr const char* DATABASE        = "\uf1c0";
    inline constexpr const char* SERVER          = "\uf233";
    inline constexpr const char* CODE            = "\uf121";
    inline constexpr const char* TERMINAL        = "\uf120";
    inline constexpr const char* CLOUD_DOWNLOAD  = "\uf381";
    inline constexpr const char* CLOUD_UPLOAD    = "\uf382";

    // Text formatting
    inline constexpr const char* FONT           = "\uf031";
    inline constexpr const char* TEXT_HEIGHT    = "\uf034";
    inline constexpr const char* TEXT_WIDTH     = "\uf035";
    inline constexpr const char* ALIGN_LEFT     = "\uf036";
    inline constexpr const char* ALIGN_CENTER   = "\uf037";
    inline constexpr const char* ALIGN_RIGHT    = "\uf038";
    inline constexpr const char* ALIGN_JUSTIFY  = "\uf039";
    inline constexpr const char* LIST           = "\uf03a";
    inline constexpr const char* LIST_UL        = "\uf0ca";
    inline constexpr const char* LIST_OL        = "\uf0cb";
    inline constexpr const char* INDENT         = "\uf03c";
    inline constexpr const char* OUTDENT        = "\uf03b";
    inline constexpr const char* TABLE          = "\uf0ce";

    // Shapes
    inline constexpr const char* CIRCLE      = "\uf111";
    inline constexpr const char* SQUARE      = "\uf0c8";
    inline constexpr const char* CHECK_SQUARE= "\uf14a";
    inline constexpr const char* MINUS_SQUARE= "\uf146";
    inline constexpr const char* PLUS_SQUARE = "\uf0fe";
    inline constexpr const char* CHECK_CIRCLE= "\uf058";

    // File types
    inline constexpr const char* FILE_AUDIO    = "\uf1c7";
    inline constexpr const char* FILE_VIDEO    = "\uf1c8";
    inline constexpr const char* FILE_IMAGE    = "\uf1c5";
    inline constexpr const char* FILE_PDF      = "\uf1c1";
    inline constexpr const char* FILE_WORD     = "\uf1c2";
    inline constexpr const char* FILE_EXCEL    = "\uf1c3";
    inline constexpr const char* FILE_POWERPOINT = "\uf1c4";
    inline constexpr const char* FILE_ARCHIVE  = "\uf1c6";

    // Additional icons used by EditorToolbar
    inline constexpr const char* MOUSE_POINTER   = "\uf245";
    inline constexpr const char* SYNC_ALT        = "\uf2f1";
    inline constexpr const char* EXPAND_ARROWS_ALT = "\uf31e";
    inline constexpr const char* STEP_FORWARD    = "\uf051";
    inline constexpr const char* FAST_FORWARD    = "\uf050";
    inline constexpr const char* FORWARD         = "\uf04e";
    inline constexpr const char* DOT_CIRCLE      = "\uf192";
    inline constexpr const char* TH              = "\uf00a";
    inline constexpr const char* TH_LARGE        = "\uf009";
    inline constexpr const char* EYEDROPPER      = "\uf1fb";
    inline constexpr const char* ERASER          = "\uf12d";
} // namespace Icons

// ==================== Theme Color Initialization ====================

void EditorStyle::InitializeThemeColors() {
    // ============================================================
    // DARK THEME (Default) - Modern dark with blue accent
    // ============================================================
    s_DarkColors = {
        // Main
        ImVec4(0.12f, 0.12f, 0.14f, 1.0f),  // bg_main
        ImVec4(0.16f, 0.16f, 0.18f, 1.0f),  // bg_panel
        ImVec4(0.24f, 0.24f, 0.28f, 1.0f),  // bg_hover
        ImVec4(0.32f, 0.32f, 0.36f, 1.0f),  // bg_active

        // Borders
        ImVec4(0.28f, 0.28f, 0.30f, 1.0f),  // border
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // border_focus

        // Text
        ImVec4(0.92f, 0.92f, 0.94f, 1.0f),  // text
        ImVec4(0.50f, 0.50f, 0.55f, 1.0f),  // text_disabled
        ImVec4(0.40f, 0.70f, 1.00f, 1.0f),  // text_link

        // Accent
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // accent
        ImVec4(0.48f, 0.68f, 0.98f, 1.0f),  // accent_hover
        ImVec4(0.28f, 0.48f, 0.78f, 1.0f),  // accent_active

        // Status
        ImVec4(0.30f, 0.75f, 0.35f, 1.0f),  // success
        ImVec4(0.95f, 0.70f, 0.15f, 1.0f),  // warning
        ImVec4(0.90f, 0.30f, 0.30f, 1.0f),  // error
        ImVec4(0.35f, 0.70f, 1.00f, 1.0f),  // info

        // Selection
        ImVec4(0.26f, 0.59f, 0.98f, 0.35f), // selection
        ImVec4(0.26f, 0.59f, 0.98f, 1.0f),  // selection_border

        // Scrollbar
        ImVec4(0.08f, 0.08f, 0.09f, 1.0f),  // scrollbar_bg
        ImVec4(0.30f, 0.30f, 0.34f, 1.0f),  // scrollbar
        ImVec4(0.40f, 0.40f, 0.44f, 1.0f),  // scrollbar_hover
        ImVec4(0.50f, 0.50f, 0.54f, 1.0f),  // scrollbar_active

        // Titlebar
        ImVec4(0.14f, 0.14f, 0.16f, 1.0f),  // title_bg
        ImVec4(0.18f, 0.18f, 0.20f, 1.0f),  // title_bg_active
        ImVec4(0.12f, 0.12f, 0.14f, 1.0f),  // title_bg_collapsed

        // Menubar
        ImVec4(0.13f, 0.13f, 0.15f, 1.0f),  // menubar_bg

        // Popup
        ImVec4(0.16f, 0.16f, 0.18f, 0.98f), // popup_bg
        ImVec4(0.28f, 0.28f, 0.30f, 1.0f),  // popup_border

        // Tab
        ImVec4(0.16f, 0.16f, 0.18f, 1.0f),  // tab
        ImVec4(0.24f, 0.24f, 0.28f, 1.0f),  // tab_hover
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // tab_active
        ImVec4(0.14f, 0.14f, 0.16f, 1.0f),  // tab_unfocused
        ImVec4(0.18f, 0.18f, 0.20f, 1.0f),  // tab_unfocused_active

        // Separator
        ImVec4(0.28f, 0.28f, 0.30f, 1.0f),  // separator

        // Checkmark
        ImVec4(0.92f, 0.92f, 0.94f, 1.0f),  // checkmark

        // Slider
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // slider_grab
        ImVec4(0.48f, 0.68f, 0.98f, 1.0f),  // slider_grab_active

        // Button
        ImVec4(0.24f, 0.24f, 0.28f, 1.0f),  // button
        ImVec4(0.32f, 0.32f, 0.36f, 1.0f),  // button_hover
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // button_active

        // Header
        ImVec4(0.24f, 0.24f, 0.28f, 1.0f),  // header
        ImVec4(0.32f, 0.32f, 0.36f, 1.0f),  // header_hover
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // header_active

        // Resize grip
        ImVec4(0.28f, 0.28f, 0.30f, 1.0f),  // resize_grip
        ImVec4(0.38f, 0.58f, 0.88f, 0.67f), // resize_grip_hover
        ImVec4(0.38f, 0.58f, 0.88f, 0.95f), // resize_grip_active

        // Plot
        ImVec4(0.61f, 0.61f, 0.64f, 1.0f),  // plot_lines
        ImVec4(1.00f, 0.43f, 0.35f, 1.0f),  // plot_lines_hover
        ImVec4(0.90f, 0.70f, 0.00f, 1.0f),  // plot_histogram
        ImVec4(1.00f, 0.60f, 0.00f, 1.0f),  // plot_histogram_hover

        // Text selected bg
        ImVec4(0.26f, 0.59f, 0.98f, 0.35f), // text_selected_bg

        // Drag drop target
        ImVec4(1.00f, 1.00f, 0.00f, 0.90f), // drag_drop_target

        // Nav
        ImVec4(0.26f, 0.59f, 0.98f, 1.0f),  // nav_highlight
        ImVec4(1.00f, 1.00f, 1.00f, 0.70f), // nav_windowing_highlight
        ImVec4(0.80f, 0.80f, 0.80f, 0.20f), // nav_windowing_dim_bg

        // Modal
        ImVec4(0.80f, 0.80f, 0.80f, 0.35f), // modal_window_dim_bg
    };

    // ============================================================
    // LIGHT THEME - Modern light with blue accent
    // ============================================================
    s_LightColors = {
        // Main
        ImVec4(0.95f, 0.95f, 0.96f, 1.0f),  // bg_main
        ImVec4(1.00f, 1.00f, 1.00f, 1.0f),  // bg_panel
        ImVec4(0.90f, 0.90f, 0.92f, 1.0f),  // bg_hover
        ImVec4(0.85f, 0.85f, 0.88f, 1.0f),  // bg_active

        // Borders
        ImVec4(0.75f, 0.75f, 0.78f, 1.0f),  // border
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // border_focus

        // Text
        ImVec4(0.15f, 0.15f, 0.18f, 1.0f),  // text
        ImVec4(0.50f, 0.50f, 0.55f, 1.0f),  // text_disabled
        ImVec4(0.30f, 0.55f, 0.90f, 1.0f),  // text_link

        // Accent (same as dark)
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // accent
        ImVec4(0.48f, 0.68f, 0.98f, 1.0f),  // accent_hover
        ImVec4(0.28f, 0.48f, 0.78f, 1.0f),  // accent_active

        // Status (same)
        ImVec4(0.30f, 0.75f, 0.35f, 1.0f),  // success
        ImVec4(0.95f, 0.70f, 0.15f, 1.0f),  // warning
        ImVec4(0.90f, 0.30f, 0.30f, 1.0f),  // error
        ImVec4(0.35f, 0.70f, 1.00f, 1.0f),  // info

        // Selection
        ImVec4(0.26f, 0.59f, 0.98f, 0.35f), // selection
        ImVec4(0.26f, 0.59f, 0.98f, 1.0f),  // selection_border

        // Scrollbar
        ImVec4(0.88f, 0.88f, 0.90f, 1.0f),  // scrollbar_bg
        ImVec4(0.60f, 0.60f, 0.65f, 1.0f),  // scrollbar
        ImVec4(0.50f, 0.50f, 0.55f, 1.0f),  // scrollbar_hover
        ImVec4(0.40f, 0.40f, 0.45f, 1.0f),  // scrollbar_active

        // Titlebar
        ImVec4(0.92f, 0.92f, 0.94f, 1.0f),  // title_bg
        ImVec4(0.88f, 0.88f, 0.90f, 1.0f),  // title_bg_active
        ImVec4(0.95f, 0.95f, 0.96f, 1.0f),  // title_bg_collapsed

        // Menubar
        ImVec4(0.94f, 0.94f, 0.95f, 1.0f),  // menubar_bg

        // Popup
        ImVec4(1.00f, 1.00f, 1.00f, 0.98f), // popup_bg
        ImVec4(0.75f, 0.75f, 0.78f, 1.0f),  // popup_border

        // Tab
        ImVec4(0.98f, 0.98f, 0.99f, 1.0f),  // tab
        ImVec4(0.90f, 0.90f, 0.92f, 1.0f),  // tab_hover
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // tab_active
        ImVec4(0.96f, 0.96f, 0.97f, 1.0f),  // tab_unfocused
        ImVec4(0.92f, 0.92f, 0.94f, 1.0f),  // tab_unfocused_active

        // Separator
        ImVec4(0.75f, 0.75f, 0.78f, 1.0f),  // separator

        // Checkmark
        ImVec4(0.15f, 0.15f, 0.18f, 1.0f),  // checkmark

        // Slider
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // slider_grab
        ImVec4(0.48f, 0.68f, 0.98f, 1.0f),  // slider_grab_active

        // Button
        ImVec4(0.90f, 0.90f, 0.92f, 1.0f),  // button
        ImVec4(0.85f, 0.85f, 0.88f, 1.0f),  // button_hover
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // button_active

        // Header
        ImVec4(0.90f, 0.90f, 0.92f, 1.0f),  // header
        ImVec4(0.85f, 0.85f, 0.88f, 1.0f),  // header_hover
        ImVec4(0.38f, 0.58f, 0.88f, 1.0f),  // header_active

        // Resize grip
        ImVec4(0.75f, 0.75f, 0.78f, 1.0f),  // resize_grip
        ImVec4(0.38f, 0.58f, 0.88f, 0.67f), // resize_grip_hover
        ImVec4(0.38f, 0.58f, 0.88f, 0.95f), // resize_grip_active

        // Plot
        ImVec4(0.40f, 0.40f, 0.43f, 1.0f),  // plot_lines
        ImVec4(1.00f, 0.43f, 0.35f, 1.0f),  // plot_lines_hover
        ImVec4(0.90f, 0.70f, 0.00f, 1.0f),  // plot_histogram
        ImVec4(1.00f, 0.60f, 0.00f, 1.0f),  // plot_histogram_hover

        // Text selected bg
        ImVec4(0.26f, 0.59f, 0.98f, 0.35f), // text_selected_bg

        // Drag drop target
        ImVec4(1.00f, 1.00f, 0.00f, 0.90f), // drag_drop_target

        // Nav
        ImVec4(0.26f, 0.59f, 0.98f, 1.0f),  // nav_highlight
        ImVec4(1.00f, 1.00f, 1.00f, 0.70f), // nav_windowing_highlight
        ImVec4(0.80f, 0.80f, 0.80f, 0.20f), // nav_windowing_dim_bg

        // Modal
        ImVec4(0.80f, 0.80f, 0.80f, 0.35f), // modal_window_dim_bg
    };

    // ============================================================
    // CLASSIC THEME - RPG Maker classic blue/gray style
    // ============================================================
    s_ClassicColors = {
        // Main - Classic RPG Maker blue/gray
        ImVec4(0.20f, 0.24f, 0.32f, 1.0f),  // bg_main
        ImVec4(0.24f, 0.28f, 0.36f, 1.0f),  // bg_panel
        ImVec4(0.32f, 0.36f, 0.44f, 1.0f),  // bg_hover
        ImVec4(0.40f, 0.44f, 0.52f, 1.0f),  // bg_active

        // Borders
        ImVec4(0.40f, 0.48f, 0.60f, 1.0f),  // border
        ImVec4(0.50f, 0.70f, 1.00f, 1.0f),  // border_focus

        // Text
        ImVec4(0.95f, 0.95f, 0.95f, 1.0f),  // text
        ImVec4(0.70f, 0.75f, 0.80f, 1.0f),  // text_disabled
        ImVec4(0.60f, 0.85f, 1.00f, 1.0f),  // text_link

        // Accent - Classic blue
        ImVec4(0.35f, 0.55f, 0.90f, 1.0f),  // accent
        ImVec4(0.45f, 0.65f, 1.00f, 1.0f),  // accent_hover
        ImVec4(0.25f, 0.45f, 0.80f, 1.0f),  // accent_active

        // Status
        ImVec4(0.30f, 0.80f, 0.40f, 1.0f),  // success
        ImVec4(1.00f, 0.75f, 0.20f, 1.0f),  // warning
        ImVec4(1.00f, 0.35f, 0.35f, 1.0f),  // error
        ImVec4(0.40f, 0.75f, 1.00f, 1.0f),  // info

        // Selection
        ImVec4(0.30f, 0.60f, 1.00f, 0.40f), // selection
        ImVec4(0.30f, 0.60f, 1.00f, 1.0f),  // selection_border

        // Scrollbar
        ImVec4(0.18f, 0.22f, 0.30f, 1.0f),  // scrollbar_bg
        ImVec4(0.40f, 0.48f, 0.60f, 1.0f),  // scrollbar
        ImVec4(0.50f, 0.58f, 0.70f, 1.0f),  // scrollbar_hover
        ImVec4(0.60f, 0.68f, 0.80f, 1.0f),  // scrollbar_active

        // Titlebar
        ImVec4(0.18f, 0.22f, 0.30f, 1.0f),  // title_bg
        ImVec4(0.22f, 0.26f, 0.34f, 1.0f),  // title_bg_active
        ImVec4(0.16f, 0.20f, 0.28f, 1.0f),  // title_bg_collapsed

        // Menubar
        ImVec4(0.16f, 0.20f, 0.28f, 1.0f),  // menubar_bg

        // Popup
        ImVec4(0.22f, 0.26f, 0.34f, 0.98f), // popup_bg
        ImVec4(0.40f, 0.48f, 0.60f, 1.0f),  // popup_border

        // Tab
        ImVec4(0.22f, 0.26f, 0.34f, 1.0f),  // tab
        ImVec4(0.30f, 0.34f, 0.42f, 1.0f),  // tab_hover
        ImVec4(0.35f, 0.55f, 0.90f, 1.0f),  // tab_active
        ImVec4(0.20f, 0.24f, 0.32f, 1.0f),  // tab_unfocused
        ImVec4(0.24f, 0.28f, 0.36f, 1.0f),  // tab_unfocused_active

        // Separator
        ImVec4(0.40f, 0.48f, 0.60f, 1.0f),  // separator

        // Checkmark
        ImVec4(0.95f, 0.95f, 0.95f, 1.0f),  // checkmark

        // Slider
        ImVec4(0.35f, 0.55f, 0.90f, 1.0f),  // slider_grab
        ImVec4(0.45f, 0.65f, 1.00f, 1.0f),  // slider_grab_active

        // Button
        ImVec4(0.30f, 0.34f, 0.42f, 1.0f),  // button
        ImVec4(0.38f, 0.42f, 0.50f, 1.0f),  // button_hover
        ImVec4(0.35f, 0.55f, 0.90f, 1.0f),  // button_active

        // Header
        ImVec4(0.30f, 0.34f, 0.42f, 1.0f),  // header
        ImVec4(0.38f, 0.42f, 0.50f, 1.0f),  // header_hover
        ImVec4(0.35f, 0.55f, 0.90f, 1.0f),  // header_active

        // Resize grip
        ImVec4(0.40f, 0.48f, 0.60f, 1.0f),  // resize_grip
        ImVec4(0.50f, 0.70f, 1.00f, 0.67f), // resize_grip_hover
        ImVec4(0.50f, 0.70f, 1.00f, 0.95f), // resize_grip_active

        // Plot
        ImVec4(0.65f, 0.65f, 0.68f, 1.0f),  // plot_lines
        ImVec4(1.00f, 0.43f, 0.35f, 1.0f),  // plot_lines_hover
        ImVec4(0.90f, 0.70f, 0.00f, 1.0f),  // plot_histogram
        ImVec4(1.00f, 0.60f, 0.00f, 1.0f),  // plot_histogram_hover

        // Text selected bg
        ImVec4(0.30f, 0.60f, 1.00f, 0.40f), // text_selected_bg

        // Drag drop target
        ImVec4(1.00f, 1.00f, 0.00f, 0.90f), // drag_drop_target

        // Nav
        ImVec4(0.50f, 0.70f, 1.00f, 1.0f),  // nav_highlight
        ImVec4(1.00f, 1.00f, 1.00f, 0.70f), // nav_windowing_highlight
        ImVec4(0.80f, 0.80f, 0.80f, 0.20f), // nav_windowing_dim_bg

        // Modal
        ImVec4(0.80f, 0.80f, 0.80f, 0.35f), // modal_window_dim_bg
    };
}

// ==================== Font Loading ====================

void EditorStyle::LoadFonts(float fontScale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const float baseSize = 14.0f * fontScale;

    // Try to load custom fonts from assets/fonts/
    // If not found, fall back to ImGui default font

    // 1. Main font (FiraCode or similar monospace)
    s_MainFont = io.Fonts->AddFontFromFileTTF("assets/fonts/FiraCode-Regular.ttf", baseSize);
    if (!s_MainFont) {
        s_MainFont = io.Fonts->AddFontFromFileTTF("assets/fonts/RobotoMono-Regular.ttf", baseSize);
    }
    if (!s_MainFont) {
        s_MainFont = io.Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMono-Regular.ttf", baseSize);
    }
    if (!s_MainFont) {
        s_MainFont = io.Fonts->AddFontDefault();
        RPG_LOG_WARN("No custom font found, using ImGui default font");
    }

    // 2. Icon font (FontAwesome 6 Free Solid) - merged with main font
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;
    iconsConfig.PixelSnapH = true;
    iconsConfig.GlyphMinAdvanceX = baseSize;
    iconsConfig.OversampleH = 2;
    iconsConfig.OversampleV = 2;

    s_IconFont = io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", baseSize, &iconsConfig, FontRanges::RANGES);
    if (!s_IconFont) {
        s_IconFont = io.Fonts->AddFontFromFileTTF("assets/fonts/FontAwesome6Free-Solid-900.otf", baseSize, &iconsConfig, FontRanges::RANGES);
    }
    if (!s_IconFont) {
        RPG_LOG_WARN("FontAwesome font not found, icons will not display");
        s_IconFont = s_MainFont;
    }

    // 3. Monospace font (smaller, for code)
    s_MonoFont = io.Fonts->AddFontFromFileTTF("assets/fonts/FiraCode-Regular.ttf", 12.0f * fontScale);
    if (!s_MonoFont) {
        s_MonoFont = io.Fonts->AddFontFromFileTTF("assets/fonts/RobotoMono-Regular.ttf", 12.0f * fontScale);
    }
    if (!s_MonoFont) {
        s_MonoFont = s_MainFont;
    }

    // 4. Large font (for headers)
    s_LargeFont = io.Fonts->AddFontFromFileTTF("assets/fonts/FiraCode-Regular.ttf", 18.0f * fontScale);
    if (!s_LargeFont) {
        s_LargeFont = s_MainFont;
    }

    // 5. Small font (for compact UI)
    s_SmallFont = io.Fonts->AddFontFromFileTTF("assets/fonts/FiraCode-Regular.ttf", 11.0f * fontScale);
    if (!s_SmallFont) {
        s_SmallFont = s_MainFont;
    }

    // Set default font and build atlas
    io.FontDefault = s_MainFont;
    io.Fonts->Build();
}

// ==================== ImGui Style Configuration ====================

void EditorStyle::ConfigureImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding
    style.WindowRounding     = 6.0f;
    style.ChildRounding      = 4.0f;
    style.FrameRounding      = 4.0f;
    style.PopupRounding      = 4.0f;
    style.ScrollbarRounding  = 6.0f;
    style.GrabRounding       = 4.0f;
    style.TabRounding        = 4.0f;

    // Alignment
    style.WindowTitleAlign   = ImVec2(0.5f, 0.5f);  // Centered window titles
    style.ButtonTextAlign    = ImVec2(0.5f, 0.5f);  // Centered button text
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);  // Left-aligned separator text
    style.SeparatorTextPadding = ImVec2(10.0f, 4.0f);

    // Anti-aliasing
    style.AntiAliasedLines     = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill      = true;
    style.CurveTessellationTol = 1.25f;

    // Spacing
    style.ItemSpacing        = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing   = ImVec2(6.0f, 4.0f);
    style.CellPadding        = ImVec2(6.0f, 4.0f);
    style.TouchExtraPadding  = ImVec2(0.0f, 0.0f);
    style.IndentSpacing      = 20.0f;
    style.ColumnsMinSpacing  = 6.0f;
    style.ScrollbarSize      = 12.0f;
    style.GrabMinSize        = 10.0f;

    // Window
    style.WindowPadding      = ImVec2(10.0f, 10.0f);
    style.WindowMinSize      = ImVec2(32.0f, 32.0f);
    style.WindowBorderSize   = 1.0f;

    // Child
    style.ChildBorderSize    = 1.0f;

    // Popup
    style.PopupBorderSize    = 1.0f;

    // Frame
    style.FramePadding       = ImVec2(8.0f, 4.0f);
    style.FrameBorderSize    = 0.0f;

    // Tab
    style.TabBorderSize      = 0.0f;

    // Separator
    style.SeparatorTextBorderSize = 2.0f;

    // Window menu button position (left side for close button)
    style.WindowMenuButtonPosition = ImGuiDir_Left;

    // Color button position (right side)
    style.ColorButtonPosition = ImGuiDir_Right;

    // Disable alpha preview in color picker (optional) - commented out as not available in all ImGui versions
    // style.ColorPickerOptions |= ImGuiColorEditFlags_NoAlpha;

    // Display safe area padding
    style.DisplaySafeAreaPadding = ImVec2(4.0f, 4.0f);
}

// ==================== Color Application ====================

void EditorStyle::ApplyColorsToImGui() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    const EditorColors& c = s_CurrentColors;

    // ===== Main Backgrounds =====
    colors[ImGuiCol_WindowBg]      = c.bg_main;
    colors[ImGuiCol_ChildBg]       = c.bg_panel;
    colors[ImGuiCol_PopupBg]       = c.popup_bg;

    // ===== Borders =====
    colors[ImGuiCol_Border]        = c.border;
    colors[ImGuiCol_BorderShadow]  = ImVec4(0, 0, 0, 0);

    // ===== Text =====
    colors[ImGuiCol_Text]              = c.text;
    colors[ImGuiCol_TextDisabled]      = c.text_disabled;
    colors[ImGuiCol_TextSelectedBg]    = c.text_selected_bg;

    // ===== Window Titles =====
    colors[ImGuiCol_TitleBg]           = c.title_bg;
    colors[ImGuiCol_TitleBgActive]     = c.title_bg_active;
    colors[ImGuiCol_TitleBgCollapsed]  = c.title_bg_collapsed;

    // ===== Menu Bar =====
    colors[ImGuiCol_MenuBarBg]         = c.menubar_bg;

    // ===== Scrollbar =====
    colors[ImGuiCol_ScrollbarBg]       = c.scrollbar_bg;
    colors[ImGuiCol_ScrollbarGrab]     = c.scrollbar;
    colors[ImGuiCol_ScrollbarGrabHovered] = c.scrollbar_hover;
    colors[ImGuiCol_ScrollbarGrabActive]  = c.scrollbar_active;

    // ===== Buttons =====
    colors[ImGuiCol_Button]        = c.button;
    colors[ImGuiCol_ButtonHovered] = c.button_hover;
    colors[ImGuiCol_ButtonActive]  = c.button_active;

    // ===== Headers (Tree nodes, selectables) =====
    colors[ImGuiCol_Header]        = c.header;
    colors[ImGuiCol_HeaderHovered] = c.header_hover;
    colors[ImGuiCol_HeaderActive]  = c.header_active;

    // ===== Separators =====
    colors[ImGuiCol_Separator]       = c.separator;
    colors[ImGuiCol_SeparatorHovered] = c.accent_hover;
    colors[ImGuiCol_SeparatorActive]  = c.accent;

    // ===== Resize Grip =====
    colors[ImGuiCol_ResizeGrip]       = c.resize_grip;
    colors[ImGuiCol_ResizeGripHovered] = c.resize_grip_hover;
    colors[ImGuiCol_ResizeGripActive]  = c.resize_grip_active;

    // ===== Tabs =====
    colors[ImGuiCol_Tab]               = c.tab;
    colors[ImGuiCol_TabHovered]        = c.tab_hover;
    colors[ImGuiCol_TabActive]         = c.tab_active;
    colors[ImGuiCol_TabUnfocused]      = c.tab_unfocused;
    colors[ImGuiCol_TabUnfocusedActive]= c.tab_unfocused_active;

    // ===== Docking =====
    colors[ImGuiCol_DockingPreview] = c.accent;
    colors[ImGuiCol_DockingEmptyBg] = c.bg_main;

    // ===== Plot =====
    colors[ImGuiCol_PlotLines]       = c.plot_lines;
    colors[ImGuiCol_PlotLinesHovered]= c.plot_lines_hover;
    colors[ImGuiCol_PlotHistogram]   = c.plot_histogram;
    colors[ImGuiCol_PlotHistogramHovered] = c.plot_histogram_hover;

    // ===== Tables =====
    colors[ImGuiCol_TableHeaderBg]   = c.header;
    colors[ImGuiCol_TableBorderStrong]= c.border;
    colors[ImGuiCol_TableBorderLight] = c.separator;
    colors[ImGuiCol_TableRowBg]      = c.bg_panel;
    colors[ImGuiCol_TableRowBgAlt]   = c.bg_hover;

    // ===== Drag & Drop =====
    colors[ImGuiCol_DragDropTarget] = c.drag_drop_target;

    // ===== Navigation =====
    colors[ImGuiCol_NavHighlight]           = c.nav_highlight;
    colors[ImGuiCol_NavWindowingHighlight]  = c.nav_windowing_highlight;
    colors[ImGuiCol_NavWindowingDimBg]      = c.nav_windowing_dim_bg;

    // ===== Modal =====
    colors[ImGuiCol_ModalWindowDimBg] = c.modal_window_dim_bg;

    // ===== Checkmark =====
    colors[ImGuiCol_CheckMark] = c.checkmark;

    // ===== Slider =====
    colors[ImGuiCol_SliderGrab]       = c.slider_grab;
    colors[ImGuiCol_SliderGrabActive] = c.slider_grab_active;

    // ===== Frame Background (for inputs) =====
    colors[ImGuiCol_FrameBg]       = c.bg_panel;
    colors[ImGuiCol_FrameBgHovered] = c.bg_hover;
    colors[ImGuiCol_FrameBgActive]  = c.bg_active;
}

// ==================== Public API ====================

void EditorStyle::Initialize(EditorTheme theme, float fontScale) {
    RPG_LOG_INFO("Initializing EditorStyle...");

    // Store font scale
    s_FontScale = std::max(0.5f, std::min(3.0f, fontScale));  // Clamp to reasonable range

    // Initialize all theme colors
    InitializeThemeColors();

    // Load fonts
    LoadFonts(s_FontScale);

    // Configure base ImGui style
    ConfigureImGuiStyle();

    // Apply theme colors
    ApplyTheme(theme);

    // Apply font scaling
    ApplyFontScaling(s_FontScale);

    RPG_LOG_INFO("EditorStyle initialized with theme: " + std::to_string(static_cast<int>(s_CurrentTheme)) +
                 ", font scale: " + std::to_string(s_FontScale));
}

void EditorStyle::Shutdown() {
    s_MainFont = nullptr;
    s_IconFont = nullptr;
    s_MonoFont = nullptr;
    s_LargeFont = nullptr;
    s_SmallFont = nullptr;
    RPG_LOG_INFO("EditorStyle shutdown complete");
}

void EditorStyle::ApplyTheme(EditorTheme theme) {
    s_CurrentTheme = theme;

    switch (theme) {
        case EditorTheme::Dark:
            s_CurrentColors = s_DarkColors;
            break;
        case EditorTheme::Light:
            s_CurrentColors = s_LightColors;
            break;
        case EditorTheme::Classic:
            s_CurrentColors = s_ClassicColors;
            break;
    }

    ApplyColorsToImGui();
    ImGui::GetIO().FontGlobalScale = s_FontScale;
}

void EditorStyle::ApplyFontScaling(float scale) {
    s_FontScale = std::max(0.5f, std::min(3.0f, scale));
    ImGui::GetIO().FontGlobalScale = s_FontScale;
}

} // namespace rpg