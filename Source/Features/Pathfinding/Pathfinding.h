#pragma once

#include "Core/Types.h"

#include <vector>

class ItemDatabase;
class Proxy;
class World;

std::vector<Vec2i> FindPath(const World& world, const ItemDatabase& items, Vec2i start, Vec2i goal, int32 userID);

void WalkTo(Proxy& proxy, Vec2i goal);
