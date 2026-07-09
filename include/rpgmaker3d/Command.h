#pragma once

#include <string>
#include "Types.h"

namespace rpg {

class Engine;

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute(Engine& engine) = 0;
    virtual void Undo(Engine& engine) = 0;
    virtual std::string GetName() const = 0;
};

class MoveEntityCommand : public ICommand {
public:
    MoveEntityCommand(EntityID id, const Vec3& oldPos, const Vec3& newPos);

    void Execute(Engine& engine) override;
    void Undo(Engine& engine) override;
    std::string GetName() const override { return "Move Entity"; }

private:
    EntityID mID;
    Vec3 mOldPos;
    Vec3 mNewPos;
};

class SetTileCommand : public ICommand {
public:
    SetTileCommand(int layer, int x, int z, int oldTile, int newTile);

    void Execute(Engine& engine) override;
    void Undo(Engine& engine) override;
    std::string GetName() const override { return "Set Tile"; }

private:
    int mLayer;
    int mX;
    int mZ;
    int mOldTile;
    int mNewTile;
};

} // namespace rpg
