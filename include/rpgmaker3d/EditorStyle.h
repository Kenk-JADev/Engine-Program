#pragma once

#include <imgui.h>
#include <string>
#include "Types.h"

namespace rpg {

// Forward declaration for theme-specific colors
struct EditorColors;

/// Available editor themes
enum class EditorTheme {
    Dark,      // Modern dark theme (default)
    Light,     // Modern light theme
    Classic    // RPG Maker classic blue/gray theme
};

/// Color palette for a specific theme
/// Contains all colors needed for ImGui styling
struct EditorColors {
    // ===== Main Background Colors =====
    ImVec4 bg_main         = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);  // Main window background
    ImVec4 bg_panel        = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);  // Panel/child window background
    ImVec4 bg_hover        = ImVec4(0.24f, 0.24f, 0.28f, 1.0f);  // Hover state background
    ImVec4 bg_active       = ImVec4(0.32f, 0.32f, 0.36f, 1.0f);  // Active/pressed state background

    // ===== Border Colors =====
    ImVec4 border          = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);  // Standard border
    ImVec4 border_focus    = ImVec4(0.38f, 0.58f, 0.88f, 1.0f);  // Focused border

    // ===== Text Colors =====
    ImVec4 text            = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);  // Primary text
    ImVec4 text_disabled   = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);  // Disabled text
    ImVec4 text_link       = ImVec4(0.40f, 0.70f, 1.00f, 1.0f);  // Link text

    // ===== Accent Colors =====
    ImVec4 accent          = ImVec4(0.38f, 0.58f, 0.88f, 1.0f);  // Primary accent
    ImVec4 accent_hover    = ImVec4(0.48f, 0.68f, 0.98f, 1.0f);  // Accent hover
    ImVec4 accent_active   = ImVec4(0.28f, 0.48f, 0.78f, 1.0f);  // Accent active

    // ===== Status Colors =====
    ImVec4 success         = ImVec4(0.30f, 0.75f, 0.35f, 1.0f);  // Success/green
    ImVec4 warning         = ImVec4(0.95f, 0.70f, 0.15f, 1.0f);  // Warning/yellow
    ImVec4 error           = ImVec4(0.90f, 0.30f, 0.30f, 1.0f);  // Error/red
    ImVec4 info            = ImVec4(0.35f, 0.70f, 1.00f, 1.0f);  // Info/blue

    // ===== Selection Colors =====
    ImVec4 selection           = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);  // Text selection
    ImVec4 selection_border    = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);   // Selection border

    // ===== Scrollbar Colors =====
    ImVec4 scrollbar_bg       = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);  // Scrollbar background
    ImVec4 scrollbar          = ImVec4(0.30f, 0.30f, 0.34f, 1.0f);  // Scrollbar grab
    ImVec4 scrollbar_hover    = ImVec4(0.40f, 0.40f, 0.44f, 1.0f);  // Scrollbar hover
    ImVec4 scrollbar_active   = ImVec4(0.50f, 0.50f, 0.54f, 1.0f);  // Scrollbar active

    // ===== Title Bar Colors =====
    ImVec4 title_bg            = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);  // Inactive title
    ImVec4 title_bg_active     = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);  // Active title
    ImVec4 title_bg_collapsed  = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);  // Collapsed title

    // ===== Menu Bar =====
    ImVec4 menubar_bg = ImVec4(0.13f, 0.13f, 0.15f, 1.0f);

    // ===== Popup/Modal =====
    ImVec4 popup_bg     = ImVec4(0.16f, 0.16f, 0.18f, 0.98f);
    ImVec4 popup_border = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);

    // ===== Tab Colors =====
    ImVec4 tab                 = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);  // Inactive tab
    ImVec4 tab_hover           = ImVec4(0.24f, 0.24f, 0.28f, 1.0f);  // Tab hover
    ImVec4 tab_active          = ImVec4(0.38f, 0.58f, 0.88f, 1.0f);  // Active tab
    ImVec4 tab_unfocused       = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);  // Unfocused tab
    ImVec4 tab_unfocused_active= ImVec4(0.18f, 0.18f, 0.20f, 1.0f);  // Unfocused active tab

    // ===== Separator =====
    ImVec4 separator = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);

    // ===== Checkmark =====
    ImVec4 checkmark = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);

    // ===== Slider =====
    ImVec4 slider_grab       = ImVec4(0.38f, 0.58f, 0.88f, 1.0f);
    ImVec4 slider_grab_active= ImVec4(0.48f, 0.68f, 0.98f, 1.0f);

    // ===== Button =====
    ImVec4 button       = ImVec4(0.24f, 0.24f, 0.28f, 1.0f);
    ImVec4 button_hover = ImVec4(0.32f, 0.32f, 0.36f, 1.0f);
    ImVec4 button_active= ImVec4(0.38f, 0.58f, 0.88f, 1.0f);

    // ===== Header (Tree nodes, etc.) =====
    ImVec4 header       = ImVec4(0.24f, 0.24f, 0.28f, 1.0f);
    ImVec4 header_hover = ImVec4(0.32f, 0.32f, 0.36f, 1.0f);
    ImVec4 header_active= ImVec4(0.38f, 0.58f, 0.88f, 1.0f);

    // ===== Resize Grip =====
    ImVec4 resize_grip       = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
    ImVec4 resize_grip_hover = ImVec4(0.38f, 0.58f, 0.88f, 0.67f);
    ImVec4 resize_grip_active= ImVec4(0.38f, 0.58f, 0.88f, 0.95f);

    // ===== Plot =====
    ImVec4 plot_lines          = ImVec4(0.61f, 0.61f, 0.64f, 1.0f);
    ImVec4 plot_lines_hover    = ImVec4(1.00f, 0.43f, 0.35f, 1.0f);
    ImVec4 plot_histogram      = ImVec4(0.90f, 0.70f, 0.00f, 1.0f);
    ImVec4 plot_histogram_hover= ImVec4(1.00f, 0.60f, 0.00f, 1.0f);

    // ===== Text Selected Background =====
    ImVec4 text_selected_bg = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

    // ===== Drag & Drop =====
    ImVec4 drag_drop_target = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);

    // ===== Navigation =====
    ImVec4 nav_highlight           = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
    ImVec4 nav_windowing_highlight = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    ImVec4 nav_windowing_dim_bg    = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

    // ===== Modal Window Dim =====
    ImVec4 modal_window_dim_bg = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
};

/// FontAwesome unicode ranges for font loading
namespace FontRanges {
    // FontAwesome 6 Free Solid: 0xF000 - 0xFAFF
    inline constexpr ImWchar ICON_MIN = 0xF000;
    inline constexpr ImWchar ICON_MAX = 0xFAFF;
    inline constexpr ImWchar RANGES[] = { ICON_MIN, ICON_MAX, 0 };
}

/// FontAwesome 6 Free Solid icon unicode mappings (extern declarations)
/// Definitions are in EditorStyle.cpp to avoid MSVC C4566 warnings
namespace Icons {
    // File/Folder operations
    extern const char* FOLDER;
    extern const char* FOLDER_OPEN;
    extern const char* FILE;
    extern const char* FILE_ALT;
    extern const char* FILE_CODE;

    // Media
    extern const char* PLAY;
    extern const char* PAUSE;
    extern const char* STOP;
    extern const char* MUSIC;
    extern const char* VOLUME_UP;
    extern const char* VOLUME_MUTE;
    extern const char* IMAGE;
    extern const char* VIDEO;
    extern const char* CAMERA;
    extern const char* FILM;

    // Editing
    extern const char* SAVE;
    extern const char* UNDO;
    extern const char* REDO;
    extern const char* PLUS;
    extern const char* MINUS;
    extern const char* TRASH;
    extern const char* EDIT;
    extern const char* COPY;
    extern const char* CUT;
    extern const char* PASTE;

    // View
    extern const char* EYE;
    extern const char* EYE_SLASH;
    extern const char* SEARCH;
    extern const char* ZOOM_IN;
    extern const char* ZOOM_OUT;
    extern const char* EXPAND;
    extern const char* COMPRESS;
    extern const char* ARROWS_ALT;

    // UI State
    extern const char* LOCK;
    extern const char* UNLOCK;
    extern const char* CHECK;
    extern const char* TIMES;
    extern const char* COG;
    extern const char* HOME;
    extern const char* BARS;

    // Warnings/Info
    extern const char* EXCLAMATION_TRIANGLE;
    extern const char* INFO_CIRCLE;
    extern const char* QUESTION_CIRCLE;
    extern const char* EXCLAMATION_CIRCLE;
    extern const char* BAN;
    extern const char* SPINNER;

    // Navigation
    extern const char* CHEVRON_UP;
    extern const char* CHEVRON_DOWN;
    extern const char* CHEVRON_LEFT;
    extern const char* CHEVRON_RIGHT;
    extern const char* ARROW_UP;
    extern const char* ARROW_DOWN;
    extern const char* ARROW_LEFT;
    extern const char* ARROW_RIGHT;
    extern const char* ANGLE_UP;
    extern const char* ANGLE_DOWN;
    extern const char* ANGLE_LEFT;
    extern const char* ANGLE_RIGHT;
    extern const char* CARET_UP;
    extern const char* CARET_DOWN;
    extern const char* CARET_LEFT;
    extern const char* CARET_RIGHT;

    // Game/Scene specific
    extern const char* CUBE;
    extern const char* PUZZLE_PIECE;
    extern const char* MAGIC;
    extern const char* LIGHTBULB;
    extern const char* PAINT_BRUSH;
    extern const char* LAYER_GROUP;
    extern const char* OBJECT_GROUP;
    extern const char* DATABASE;
    extern const char* SERVER;
    extern const char* CODE;
    extern const char* TERMINAL;
    extern const char* CLOUD_DOWNLOAD;
    extern const char* CLOUD_UPLOAD;

    // Text formatting
    extern const char* FONT;
    extern const char* TEXT_HEIGHT;
    extern const char* TEXT_WIDTH;
    extern const char* ALIGN_LEFT;
    extern const char* ALIGN_CENTER;
    extern const char* ALIGN_RIGHT;
    extern const char* ALIGN_JUSTIFY;
    extern const char* LIST;
    extern const char* LIST_UL;
    extern const char* LIST_OL;
    extern const char* INDENT;
    extern const char* OUTDENT;
    extern const char* TABLE;

    // Shapes
    extern const char* CIRCLE;
    extern const char* SQUARE;
    extern const char* CHECK_SQUARE;
    extern const char* MINUS_SQUARE;
    extern const char* PLUS_SQUARE;
    extern const char* CHECK_CIRCLE;

    // File types
    extern const char* FILE_AUDIO;
    extern const char* FILE_VIDEO;
    extern const char* FILE_IMAGE;
    extern const char* FILE_PDF;
    extern const char* FILE_WORD;
    extern const char* FILE_EXCEL;
    extern const char* FILE_POWERPOINT;
    extern const char* FILE_ARCHIVE;

    // Additional icons used by EditorToolbar
    extern const char* MOUSE_POINTER;
    extern const char* SYNC_ALT;
    extern const char* EXPAND_ARROWS_ALT;
    extern const char* STEP_FORWARD;
    extern const char* FAST_FORWARD;
    extern const char* FORWARD;
    extern const char* DOT_CIRCLE;
    extern const char* TH;
    extern const char* TH_LARGE;
    extern const char* EYEDROPPER;
    extern const char* ERASER;
}

/// Main editor styling system
/// Handles themes, fonts, colors, and ImGui style configuration
class EditorStyle {
public:
    // ===== Initialization =====
    /// Initialize the editor style system with a specific theme
    /// @param theme The theme to apply (default: Dark)
    /// @param fontScale Global font scaling factor (default: 1.0)
    static void Initialize(EditorTheme theme = EditorTheme::Dark, float fontScale = 1.0f);

    /// Shutdown and clean up resources
    static void Shutdown();

    // ===== Theme Management =====
    /// Apply a theme at runtime
    static void ApplyTheme(EditorTheme theme);

    /// Get the currently active theme
    static EditorTheme GetCurrentTheme() { return s_CurrentTheme; }

    /// Apply font scaling globally
    static void ApplyFontScaling(float scale);

    /// Get current font scale
    static float GetFontScale() { return s_FontScale; }

    // ===== Color Access =====
    /// Get the current color palette
    static const EditorColors& GetColors() { return s_CurrentColors; }

    // ===== Font Access =====
    static ImFont* GetMainFont()   { return s_MainFont; }
    static ImFont* GetIconFont()   { return s_IconFont; }
    static ImFont* GetMonoFont()   { return s_MonoFont; }
    static ImFont* GetLargeFont()  { return s_LargeFont; }
    static ImFont* GetSmallFont()  { return s_SmallFont; }

    // ===== Style Helpers =====
    /// Push a style variable (float)
    static void PushStyleVar(ImGuiStyleVar idx, float val) { ImGui::PushStyleVar(idx, val); }

    /// Push a style variable (ImVec2)
    static void PushStyleVar(ImGuiStyleVar idx, const ImVec2& val) { ImGui::PushStyleVar(idx, val); }

    /// Pop style variables
    static void PopStyleVar(int count = 1) { ImGui::PopStyleVar(count); }

    /// Push a style color
    static void PushColor(ImGuiCol idx, const ImVec4& color) { ImGui::PushStyleColor(idx, color); }

    /// Pop style colors
    static void PopColor(int count = 1) { ImGui::PopStyleColor(count); }

    /// Helper: Push theme accent color
    static void PushAccentColor() {
        PushColor(ImGuiCol_Button, s_CurrentColors.accent);
        PushColor(ImGuiCol_ButtonHovered, s_CurrentColors.accent_hover);
        PushColor(ImGuiCol_ButtonActive, s_CurrentColors.accent_active);
    }

    /// Helper: Pop theme accent color
    static void PopAccentColor() { PopColor(3); }

    /// Helper: Push error color
    static void PushErrorColor() { PushColor(ImGuiCol_Text, s_CurrentColors.error); }

    /// Helper: Push success color
    static void PushSuccessColor() { PushColor(ImGuiCol_Text, s_CurrentColors.success); }

    /// Helper: Push warning color
    static void PushWarningColor() { PushColor(ImGuiCol_Text, s_CurrentColors.warning); }

    /// Helper: Pop text color
    static void PopTextColor() { PopColor(1); }

private:
    // ===== Internal Implementation =====
    static void InitializeThemeColors();
    static void LoadFonts(float fontScale);
    static void ApplyColorsToImGui();
    static void ConfigureImGuiStyle();

    // ===== Static Members =====
    static EditorColors s_DarkColors;
    static EditorColors s_LightColors;
    static EditorColors s_ClassicColors;
    static EditorColors s_CurrentColors;
    static EditorTheme  s_CurrentTheme;
    static float        s_FontScale;

    static ImFont* s_MainFont;
    static ImFont* s_IconFont;
    static ImFont* s_MonoFont;
    static ImFont* s_LargeFont;
    static ImFont* s_SmallFont;
};

} // namespace rpg