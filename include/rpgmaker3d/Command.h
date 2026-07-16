#pragma once

#include <string>
#include <vector>
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

class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(const std::string& name);

    void Execute(Engine& engine) override;
    void Undo(Engine& engine) override;
    std::string GetName() const override { return "Create Entity"; }
    EntityID GetEntityID() const { return mID; }

private:
    EntityID mID = INVALID_ENTITY;
    std::string mName;
};

class DeleteEntityCommand : public ICommand {
public:
    DeleteEntityCommand(EntityID id, const std::string& name, const Transform& transform);

    void Execute(Engine& engine) override;
    void Undo(Engine& engine) override;
    std::string GetName() const override { return "Delete Entity"; }

private:
    EntityID mID;
    std::string mName;
    Transform mTransform;
};

// Mehrere Tiles in einem Undo-Schritt (Rechteck-Pinsel / Fill)
class BatchTileCommand : public ICommand {
public:
    struct Change {
        int layer = 0;
        int x = 0;
        int z = 0;
        int oldTile = -1;
        int newTile = -1;
    };

    explicit BatchTileCommand(std::vector<Change> changes, std::string name = "Paint Tiles");

    void Execute(Engine& engine) override;
    void Undo(Engine& engine) override;
    std::string GetName() const override { return mName; }
    size_t Size() const { return mChanges.size(); }

private:
    std::vector<Change> mChanges;
    std::string mName;
};

} // namespace rpg
