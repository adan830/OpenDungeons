/*
 *  Copyright (C) 2011-2015  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rooms/RoomHatchery.h"

#include "entities/Tile.h"
#include "entities/ChickenEntity.h"
#include "game/Player.h"
#include "gamemap/GameMap.h"
#include "rooms/RoomManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

const std::string RoomHatcheryName = "Hatchery";
const std::string RoomHatcheryNameDisplay = "Hatchery room";
const RoomType RoomHatchery::mRoomType = RoomType::hatchery;

namespace
{
class RoomHatcheryFactory : public RoomFactory
{
    RoomType getRoomType() const override
    { return RoomHatchery::mRoomType; }

    const std::string& getName() const override
    { return RoomHatcheryName; }

    const std::string& getNameReadable() const override
    { return RoomHatcheryNameDisplay; }

    virtual void checkBuildRoom(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const
    {
        Room::checkBuildRoomDefault(gameMap, RoomHatchery::mRoomType, inputManager, inputCommand);
    }

    virtual bool buildRoom(GameMap* gameMap, Player* player, ODPacket& packet) const
    {
        std::vector<Tile*> tiles;
        if(!Room::getRoomTilesDefault(tiles, gameMap, player, packet))
            return false;

        int32_t pricePerTarget = RoomManager::costPerTile(RoomHatchery::mRoomType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
            return false;

        RoomHatchery* room = new RoomHatchery(gameMap);
        return Room::buildRoomDefault(gameMap, room, player->getSeat(), tiles);
    }

    virtual void checkBuildRoomEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const
    {
        Room::checkBuildRoomDefaultEditor(gameMap, RoomHatchery::mRoomType, inputManager, inputCommand);
    }

    virtual bool buildRoomEditor(GameMap* gameMap, ODPacket& packet) const
    {
        RoomHatchery* room = new RoomHatchery(gameMap);
        return Room::buildRoomDefaultEditor(gameMap, room, packet);
    }

    Room* getRoomFromStream(GameMap* gameMap, std::istream& is) const override
    {
        RoomHatchery* room = new RoomHatchery(gameMap);
        if(!Room::importRoomFromStream(*room, is))
        {
            OD_LOG_ERR("Error while building a room from the stream");
        }
        return room;
    }
};

// Register the factory
static RoomRegister reg(new RoomHatcheryFactory);
}

RoomHatchery::RoomHatchery(GameMap* gameMap) :
    Room(gameMap),
    mSpawnChickenCooldown(0)
{
    setMeshName("Farm");
}

RenderedMovableEntity* RoomHatchery::notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile)
{
    // We add chicken coops on center tiles only
    if(place == ActiveSpotPlace::activeSpotCenter)
        return loadBuildingObject(getGameMap(), "ChickenCoop", tile, 0.0, false);

    return nullptr;
}

void RoomHatchery::notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile)
{
    if(place == ActiveSpotPlace::activeSpotCenter)
    {
        // We remove the chicken coop
        removeBuildingObject(tile);
    }
}

uint32_t RoomHatchery::getNbChickens()
{
    std::vector<GameEntity*> chickens;
    for(Tile* tile : mCoveredTiles)
    {
        tile->fillWithChickenEntities(chickens);
    }

    return chickens.size();
}

void RoomHatchery::doUpkeep()
{
    Room::doUpkeep();

    if(mCoveredTiles.empty())
        return;

    uint32_t nbChickens = getNbChickens();
    if(nbChickens >= mNumActiveSpots)
        return;

    // Chickens have been eaten. We check when we will spawn another one
    ++mSpawnChickenCooldown;
    if(mSpawnChickenCooldown < ConfigManager::getSingleton().getRoomConfigUInt32("HatcheryChickenSpawnRate"))
        return;

    // We spawn 1 chicken per chicken coop (until chickens are maxed)
    for(Tile* chickenCoopTile : mCentralActiveSpotTiles)
    {
        ChickenEntity* chicken = new ChickenEntity(getGameMap(), true, getName());
        chicken->addToGameMap();
        chicken->createMesh();
        Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(chickenCoopTile->getX()),
                                    static_cast<Ogre::Real>(chickenCoopTile->getY()), 0.0f);
        chicken->setPosition(spawnPosition);
        ++nbChickens;
        if(nbChickens >= mNumActiveSpots)
            break;
    }

    mSpawnChickenCooldown = 0;
}

bool RoomHatchery::hasOpenCreatureSpot(Creature* c)
{
    return mNumActiveSpots > mCreaturesUsingRoom.size();
}

bool RoomHatchery::buildRoomOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles)
{
    int32_t pricePerTarget = RoomManager::costPerTile(RoomHatchery::mRoomType);
    int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
    if(!gameMap->withdrawFromTreasuries(price, player->getSeat()))
        return false;

    RoomHatchery* room = new RoomHatchery(gameMap);
    return buildRoomDefault(gameMap, room, player->getSeat(), tiles);
}
