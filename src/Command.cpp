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

} // namespace rpg
