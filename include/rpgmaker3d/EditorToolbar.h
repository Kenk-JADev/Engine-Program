#pragma once

#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <imgui.h>

namespace rpg {

class Engine;

struct ToolbarButton {
    std::string id;
    const char* icon;
    const char* tooltip;
    std::function<void()> onClick;
    bool enabled = true;
    bool toggled = false;
    bool toggleable = false;
    bool defaultToggled = false;
};

struct ToolbarSeparator {};

class EditorToolbar {
public:
    EditorToolbar(Engine& engine);
    ~EditorToolbar() = default;

    void Initialize();
    void Shutdown();
    void Draw();

    void AddButton(const ToolbarButton& button);
    void AddSeparator();

    void SetButtonEnabled(const std::string& id, bool enabled);
    void SetButtonToggled(const std::string& id, bool toggled);
    bool IsButtonToggled(const std::string& id) const;

    void SetupDefaultLayout();
    void SetupPlayLayout();
    void SetupEditLayout();

private:
    void DrawButton(const ToolbarButton& button);
    void DrawSeparator();

    Engine& m_Engine;
    std::vector<std::variant<ToolbarButton, ToolbarSeparator>> m_Items;
    float m_ButtonSize = 28.0f;
    float m_Spacing = 4.0f;
};

} // namespace rpg