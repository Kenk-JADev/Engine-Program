#include "rpgmaker3d/Command.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Logger.h"

namespace rpg {

MoveEntityCommand::MoveEntityCommand(EntityID id, const Vec3& oldPos, const Vec3& newPos)
    : mID(id), mOldPos(oldPos), mNewPos(newPos) {
}

void MoveEntityCommand::Execute(Engine& engine) {
    auto* transform = engine.GetScene().GetComponent<TransformComponent>(mID);
    if (transform) {
        transform->transform.position = mNewPos;
    }
}

void MoveEntityCommand::Undo(Engine& engine) {
    auto* transform = engine.GetScene().GetComponent<TransformComponent>(mID);
    if (transform) {
        transform->transform.position = mOldPos;
    }
}

SetTileCommand::SetTileCommand(int layer, int x, int z, int oldTile, int newTile)
    : mLayer(layer), mX(x), mZ(z), mOldTile(oldTile), mNewTile(newTile) {
}

void SetTileCommand::Execute(Engine& engine) {
    engine.GetMap().SetTile(mLayer, mX, mZ, mNewTile);
}

void SetTileCommand::Undo(Engine& engine) {
    engine.GetMap().SetTile(mLayer, mX, mZ, mOldTile);
}

CreateEntityCommand::CreateEntityCommand(const std::string& name)
    : mName(name) {
}

void CreateEntityCommand::Execute(Engine& engine) {
    mID = engine.GetScene().CreateEntity(mName);
}

void CreateEntityCommand::Undo(Engine& engine) {
    if (mID != INVALID_ENTITY) {
        engine.GetScene().DestroyEntity(mID);
    }
}

DeleteEntityCommand::DeleteEntityCommand(EntityID id, const std::string& name, const Transform& transform)
    : mID(id), mName(name), mTransform(transform) {
}

void DeleteEntityCommand::Execute(Engine& engine) {
    engine.GetScene().DestroyEntity(mID);
}

void DeleteEntityCommand::Undo(Engine& engine) {
    EntityID id = engine.GetScene().CreateEntity(mName);
    auto* t = engine.GetScene().AddComponent<TransformComponent>(id);
    t->transform = mTransform;
    // Note: model/components not restored; simplified undo for now
    (void)mID;
}

} // namespace rpg
