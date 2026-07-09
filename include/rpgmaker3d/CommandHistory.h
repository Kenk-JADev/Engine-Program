#pragma once

#include <memory>
#include <vector>
#include <string>

namespace rpg {

class ICommand;
class Engine;

class CommandHistory {
public:
    void Execute(Engine& engine, std::shared_ptr<ICommand> command);
    void Undo(Engine& engine);
    void Redo(Engine& engine);

    bool CanUndo() const { return mIndex > 0; }
    bool CanRedo() const { return mIndex < static_cast<int>(mCommands.size()); }

    std::string GetUndoName() const;
    std::string GetRedoName() const;

    void Clear();

private:
    std::vector<std::shared_ptr<ICommand>> mCommands;
    int mIndex = 0;
};

} // namespace rpg
