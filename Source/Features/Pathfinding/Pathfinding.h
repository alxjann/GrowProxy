#pragma once

#include "Core/Types.h"

#include <vector>

class ItemDatabase;
class Proxy;
class World;

// Shortest walkable path (4 directions, start and goal included); empty if there is none.
// playerTileY decides one-way platforms: they block depending on whether the player is above or below.
std::vector<Vec2i> FindPath(const World& world, const ItemDatabase& items, Vec2i start, Vec2i goal, int32 playerTileY);

// Moves the local player to a tile by sending a quick series of position updates along the path.
void WalkTo(Proxy& proxy, Vec2i goal);
