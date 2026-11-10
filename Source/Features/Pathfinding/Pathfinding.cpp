#include "Features/Pathfinding/Pathfinding.h"

#include "Proxy/Proxy.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace {

    constexpr uint32 STEP_INTERVAL_MS = 2;
    constexpr size_t LONG_PATH = 150; // longer paths only send every second step
    constexpr int32 WALK_STATE_ITEM = 6326;
    constexpr uint32 WALK_STATE_FLAGS = 32 | 128 | (1 << 10) | (1 << 11);

    // Whether the player can stand in a tile.
    bool IsBlocked(const Tile& tile, int32 y, const ItemDatabase& items, int32 playerTileY) {
        const Item* item = items.Get(tile.foreground);
        switch (item ? item->collisionType : COLLISION_NONE) {
            case COLLISION_NONE:
                return false;
            case COLLISION_SOLID:
                return true;
            case COLLISION_JUMP_THROUGH:
                return playerTileY <= y;
            case COLLISION_GATEWAY:
                return tile.flags != 34 && tile.flags != 128; // open doors/entrances
            case COLLISION_IF_OFF:
                return !tile.HasFlag(TILE_FLAG_IS_ON) && tile.flags != 80;
            case COLLISION_JUMP_DOWN:
                return playerTileY >= y;
            default:
                return tile.foreground != 0;
        }
    }

}

std::vector<Vec2i> FindPath(const World& world, const ItemDatabase& items, Vec2i start, Vec2i goal, int32 playerTileY) {
    const int32 width = world.GetWidth();
    const int32 height = world.GetHeight();
    auto inside = [&](Vec2i p) { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; };
    auto index = [&](Vec2i p) { return (size_t)p.y * width + p.x; };

    if (!inside(start) || !inside(goal))
        return {};

    std::vector<bool> blocked(world.GetTiles().size());
    for (int32 y = 0; y < height; ++y) {
        for (int32 x = 0; x < width; ++x)
            blocked[index({x, y})] = IsBlocked(*world.GetTile(x, y), y, items, playerTileY);
    }

    // A* with Manhattan distance
    struct Node {
        int32 cost; // distance so far + estimate
        int32 distance;
        Vec2i pos;
        bool operator>(const Node& other) const { return cost > other.cost; }
    };
    auto estimate = [&](Vec2i p) { return std::abs(goal.x - p.x) + std::abs(goal.y - p.y); };

    std::vector<int32> distance(blocked.size(), INT32_MAX);
    std::vector<int32> cameFrom(blocked.size(), -1);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    distance[index(start)] = 0;
    open.push({estimate(start), 0, start});

    const Vec2i directions[] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
    while (!open.empty()) {
        Node current = open.top();
        open.pop();

        if (current.pos.x == goal.x && current.pos.y == goal.y) {
            std::vector<Vec2i> path;
            for (int32 i = (int32)index(goal); i != -1; i = cameFrom[i])
                path.push_back({i % width, i / width});
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (current.distance > distance[index(current.pos)])
            continue;

        for (Vec2i d : directions) {
            Vec2i next{current.pos.x + d.x, current.pos.y + d.y};
            if (!inside(next) || blocked[index(next)] || current.distance + 1 >= distance[index(next)])
                continue;

            distance[index(next)] = current.distance + 1;
            cameFrom[index(next)] = (int32)index(current.pos);
            open.push({current.distance + 1 + estimate(next), current.distance + 1, next});
        }
    }
    return {};
}

void WalkTo(Proxy& proxy, Vec2i goal) {
    GameState& state = proxy.State();
    World& world = state.world;
    if (!world.IsLoaded())
        return;

    if (goal.x < 0 || goal.y < 0 || goal.x >= world.GetWidth() || goal.y >= world.GetHeight()) {
        proxy.TextOverlay("`4Target Coordinates Out of Bounds");
        return;
    }

    Vec2i start{(int32)std::floor(state.local.pos.x / 32.0f), (int32)std::floor(state.local.pos.y / 32.0f)};
    start.x = std::clamp(start.x, 0, world.GetWidth() - 1);
    start.y = std::clamp(start.y, 0, world.GetHeight() - 1);

    std::vector<Vec2i> path = FindPath(world, state.items, start, goal, start.y);
    if (path.empty()) {
        proxy.TextOverlay("`4Path Not Found");
        return;
    }

    // tell the server we walked the path, one tile at a time
    size_t stride = path.size() < LONG_PATH ? 1 : 2;
    uint32 delay = 0;
    for (size_t i = 0; i < path.size(); i += stride) {
        Vec2i tile = path[i];
        proxy.Schedule(delay, [&proxy, tile]() {
            GameUpdatePacket step;
            step.type = PACKET_STATE;
            step.value = WALK_STATE_ITEM;
            step.pos = {tile.x * 32.0f, tile.y * 32.0f};
            step.speed = step.pos;
            step.tileX = tile.x;
            step.tileY = tile.y;
            step.flags = WALK_STATE_FLAGS;
            proxy.Send(Server, step);
        });
        delay += STEP_INTERVAL_MS;
    }

    // then move the player on the client
    size_t length = path.size();
    proxy.Schedule(delay + STEP_INTERVAL_MS, [&proxy, goal, length]() {
        Vec2f pos{goal.x * 32.0f, goal.y * 32.0f};
        proxy.Send(Client, VariantList{"OnSetPos", pos}, proxy.State().local.netID);
        proxy.State().local.pos = pos;
        proxy.TextOverlay("`2Travelled `9" + std::to_string(length) + " `2Blocks");
    });
}
