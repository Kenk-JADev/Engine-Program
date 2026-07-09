#include "rpgmaker3d/CommandHistory.h"
#include "rpgmaker3d/Command.h"
#include "rpgmaker3d/Engine.h"

namespace rpg {

void CommandHistory::Execute(Engine& engine, std::shared_ptr<ICommand> command) {
    if (!command) return;

    if (mIndex < static_cast<int>(mCommands.size())) {
        mCommands.erase(mCommands.begin() + mIndex, mCommands.end());
    }

    command->Execute(engine);
    mCommands.push_back(command);
    mIndex++;

    if (mCommands.size() > 100) {
        mCommands.erase(mCommands.begin());
        mIndex--;
    }
}

void CommandHistory::Undo(Engine& engine) {
    if (!CanUndo()) return;
    mIndex--;
    mCommands[mIndex]->Undo(engine);
}

void CommandHistory::Redo(Engine& engine) {
    if (!CanRedo()) return;
    mCommands[mIndex]->Execute(engine);
    mIndex++;
}

std::string CommandHistory::GetUndoName() const {
    if (!CanUndo()) return "";
    return mCommands[mIndex - 1]->GetName();
}

std::string CommandHistory::GetRedoName() const {
    if (!CanRedo()) return "";
    return mCommands[mIndex]->GetName();
}

void CommandHistory::Clear() {
    mCommands.clear();
    mIndex = 0;
}

} // namespace rpg
