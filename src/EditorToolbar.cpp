#include "rpgmaker3d/EditorToolbar.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/EditorStyle.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/CommandHistory.h"
#include <imgui.h>
#include <algorithm>

namespace rpg {

EditorToolbar::EditorToolbar(Engine& engine) : m_Engine(engine) {
}

void EditorToolbar::Initialize() {
    SetupDefaultLayout();
}

void EditorToolbar::Shutdown() {
    m_Items.clear();
}

void EditorToolbar::Draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_Spacing, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    
    // Toolbar background
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::GetColors().button_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::GetColors().button_active);
    
    ImGui::BeginChild("##Toolbar", ImVec2(0, 40.0f), false, 
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    for (const auto& item : m_Items) {
        if (std::holds_alternative<ToolbarButton>(item)) {
            DrawButton(std::get<ToolbarButton>(item));
        } else if (std::holds_alternative<ToolbarSeparator>(item)) {
            DrawSeparator();
        }
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void EditorToolbar::DrawButton(const ToolbarButton& button) {
    ImGui::BeginDisabled(!button.enabled);
    
    bool isToggled = button.toggled;
    ImVec4 btnColor = button.toggled ? EditorStyle::GetColors().accent : ImVec4(0, 0, 0, 0);
    
    ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::GetColors().button_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::GetColors().button_active);
    
    ImVec2 btnSize(m_ButtonSize, m_ButtonSize);
    
    // Use icon font
    ImGui::PushFont(EditorStyle::GetIconFont() ? EditorStyle::GetIconFont() : ImGui::GetFont());
    
    bool clicked = ImGui::Button(button.icon, btnSize);
    
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    
    if (clicked && button.onClick) {
        button.onClick();
    }
    
    // Tooltip
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", button.tooltip);
    }
    
    ImGui::EndDisabled();
    
    ImGui::SameLine();
}

void EditorToolbar::DrawSeparator() {
    ImGui::Dummy(ImVec2(8.0f, 0.0f));
    ImGui::SameLine();
    
    ImVec2 p1 = ImGui::GetCursorScreenPos();
    ImVec2 p2 = ImVec2(p1.x, p1.y + 28.0f);
    
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(p1.x + 4.0f, p1.y + 6.0f),
        ImVec2(p2.x + 4.0f, p2.y - 6.0f),
        ImGui::GetColorU32(EditorStyle::GetColors().separator)
    );
    
    ImGui::Dummy(ImVec2(8.0f, 0.0f));
    ImGui::SameLine();
}

void EditorToolbar::AddButton(const ToolbarButton& button) {
    m_Items.emplace_back(button);
}

void EditorToolbar::AddSeparator() {
    m_Items.emplace_back(ToolbarSeparator{});
}

void EditorToolbar::SetButtonEnabled(const std::string& id, bool enabled) {
    for (auto& item : m_Items) {
        if (std::holds_alternative<ToolbarButton>(item)) {
            auto& button = std::get<ToolbarButton>(item);
            if (button.id == id) {
                button.enabled = enabled;
                break;
            }
        }
    }
}

void EditorToolbar::SetButtonToggled(const std::string& id, bool toggled) {
    for (auto& item : m_Items) {
        if (std::holds_alternative<ToolbarButton>(item)) {
            auto& button = std::get<ToolbarButton>(item);
            if (button.id == id) {
                button.toggled = toggled;
                break;
            }
        }
    }
}

bool EditorToolbar::IsButtonToggled(const std::string& id) const {
    for (const auto& item : m_Items) {
        if (std::holds_alternative<ToolbarButton>(item)) {
            const auto& button = std::get<ToolbarButton>(item);
            if (button.id == id) {
                return button.toggled;
            }
        }
    }
    return false;
}

void EditorToolbar::SetupDefaultLayout() {
    m_Items.clear();
    
    // File operations
    AddButton({"file_new", Icons::FILE, "Neues Projekt (Ctrl+N)", [this]() {
        m_Engine.GetProject().New("./NewRPGProject", "Neues RPG");
    }});
    AddButton({"file_open", Icons::FOLDER, "Projekt öffnen (Ctrl+O)", [this]() {
        m_Engine.GetProject().Load("./SampleProject");
    }});
    AddButton({"file_save", Icons::SAVE, "Projekt speichern (Ctrl+S)", [this]() {
        m_Engine.GetProject().Save();
    }});
    
    AddSeparator();
    
    // Edit operations
    AddButton({"edit_undo", Icons::UNDO, "Rückgängig (Ctrl+Z)", [this]() {
        if (m_Engine.GetCommandHistory().CanUndo()) {
            m_Engine.GetCommandHistory().Undo(m_Engine);
        }
    }, true, false, false});

    AddButton({"edit_redo", Icons::REDO, "Wiederholen (Ctrl+Y)", [this]() {
        if (m_Engine.GetCommandHistory().CanRedo()) {
            m_Engine.GetCommandHistory().Redo(m_Engine);
        }
    }, true, false, false});
    
    AddSeparator();
    
    // Create objects
    AddButton({"create_cube", Icons::CUBE, "Würfel erstellen", [this]() {
        // Implemented in Editor
    }});
    AddButton({"create_plane", Icons::TH_LARGE, "Ebene erstellen", [this]() {}});
    AddButton({"create_light", Icons::LIGHTBULB, "Licht erstellen", [this]() {}});
    
    AddSeparator();
    
    // Play controls
    AddButton({"play", Icons::PLAY, "Spiel starten (F5)", [this]() {
        m_Engine.SetPlaying(!m_Engine.IsPlaying());
    }, true, true, m_Engine.IsPlaying()});

    AddButton({"pause", Icons::PAUSE, "Pause", [this]() {
        // Pause logic
    }});
    AddButton({"stop", Icons::STOP, "Stop (Shift+F5)", [this]() {
        m_Engine.SetPlaying(false);
    }});
    
    AddSeparator();
    
    // View options
    AddButton({"view_grid", Icons::TH, "Gitter anzeigen", [this]() {}, true, true, true});
    AddButton({"view_gizmo", Icons::ARROWS_ALT, "Gizmo anzeigen", [this]() {}, true, true, true});
    
    AddSeparator();
    
    // Windows
    AddButton({"window_hierarchy", Icons::LIST, "Hierarchie", [this]() {}});
    AddButton({"window_inspector", Icons::COG, "Inspector", [this]() {}});
    AddButton({"window_project", Icons::FOLDER, "Projekt", [this]() {}});
    AddButton({"window_console", Icons::TERMINAL, "Konsole", [this]() {}});
}

void EditorToolbar::SetupPlayLayout() {
    m_Items.clear();
    
    // Minimal play layout
    AddButton({"play", Icons::PLAY, "Starten", [this]() {
        m_Engine.SetPlaying(true);
    }});
    AddButton({"pause", Icons::PAUSE, "Pause", [this]() {}});
    AddButton({"stop", Icons::STOP, "Stop", [this]() {
        m_Engine.SetPlaying(false);
    }});
    
    AddSeparator();

    AddButton({"time_scale_1", Icons::STEP_FORWARD, "1x Geschwindigkeit", [this]() {}});
    AddButton({"time_scale_2", Icons::FAST_FORWARD, "2x Geschwindigkeit", [this]() {}});
    AddButton({"time_scale_4", Icons::FORWARD, "4x Geschwindigkeit", [this]() {}});
    
    AddSeparator();

    AddButton({"debug_overlay", Icons::INFO_CIRCLE, "Debug Overlay", [this]() {}});
    AddButton({"console", Icons::TERMINAL, "Konsole", [this]() {}});
}

void EditorToolbar::SetupEditLayout() {
    m_Items.clear();
    
    // File
    AddButton({"file_new", Icons::FILE, "Neu", [this]() {}});
    AddButton({"file_open", Icons::FOLDER, "Öffnen", [this]() {}});
    AddButton({"file_save", Icons::SAVE, "Speichern", [this]() {}});
    
    AddSeparator();
    
    // Edit
    AddButton({"edit_undo", Icons::UNDO, "Rückgängig", [this]() {}});
    AddButton({"edit_redo", Icons::REDO, "Wiederholen", [this]() {}});
    
    AddSeparator();
    
    // Tools
    AddButton({"tool_select", Icons::MOUSE_POINTER, "Auswählen (V)", [this]() {}, true, true, true});
    AddButton({"tool_move", Icons::ARROWS_ALT, "Bewegen (W)", [this]() {}, true, true, false});
    AddButton({"tool_rotate", Icons::SYNC_ALT, "Drehen (E)", [this]() {}, true, true, false});
    AddButton({"tool_scale", Icons::EXPAND_ARROWS_ALT, "Skalieren (R)", [this]() {}, true, true, false});
    
    AddSeparator();
    
    AddButton({"tool_paint", Icons::PAINT_BRUSH, "Tile Malen (B)", [this]() {}, true, true, false});
    AddButton({"tool_eraser", Icons::ERASER, "Radierer (Shift+B)", [this]() {}, true, true, false});
    AddButton({"tool_eyedropper", Icons::EYEDROPPER, "Pipette (I)", [this]() {}, true, true, false});
    
    AddSeparator();
    
    AddButton({"snap_grid", Icons::TH, "Grid Snap", [this]() {}, true, true, true});
    AddButton({"snap_vertex", Icons::DOT_CIRCLE, "Vertex Snap", [this]() {}, true, true, false});
    AddButton({"snap_surface", Icons::CUBE, "Surface Snap", [this]() {}, true, true, false});
}

} // namespace rpg