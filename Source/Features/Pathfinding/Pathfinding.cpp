#include "Features/Pathfinding/Pathfinding.h"

#include "Proxy/Proxy.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace {

    constexpr int STEP_INTERVAL_MS = 2;
    constexpr size_t LONG_PATH = 150; // longer paths only send every second step
    constexpr int WALK_STATE_ITEM = 6326;
    constexpr int WALK_STATE_FLAGS = 32 | 128 | (1 << 10) | (1 << 11);

    enum TileRule : uint8 {
        RULE_OPEN,       // from anywhere
        RULE_BLOCKED,    // never
        RULE_NOT_DOWN,   // platform: solid from above, so not by moving down into it
        RULE_NOT_UP,     // solid from below, so not by moving up into it
        RULE_RIGHT_ONLY, // one-way block facing right
        RULE_LEFT_ONLY,  // one-way block facing left (flipped)
    };

    TileRule GetTileRule(const World& world, const ItemDatabase& items, int x, int y, int userID) {
        const Tile& tile = *world.GetTile(x, y);
        if (tile.foreground == 0)
            return RULE_OPEN;

        const Item* item = items.Get(tile.foreground);
        if (!item)
            return RULE_BLOCKED;

        if (item->type == ITEM_TYPE_VIP_DOOR || item->type == ITEM_TYPE_FRIENDS_ENTRANCE)
            return world.CanUseEntrance(x, y, userID) ? RULE_OPEN : RULE_BLOCKED;

        switch (item->collisionType) {
            case COLLISION_NONE:
                return RULE_OPEN;
            case COLLISION_SOLID:
                return RULE_BLOCKED;
            case COLLISION_JUMP_THROUGH:
                return RULE_NOT_DOWN;
            case COLLISION_GATEWAY:
            case COLLISION_VIP:
                return world.CanUseEntrance(x, y, userID) ? RULE_OPEN : RULE_BLOCKED;
            case COLLISION_IF_OFF:
                return tile.HasFlag(TILE_FLAG_IS_ON) ? RULE_OPEN : RULE_BLOCKED;
            case COLLISION_ONE_WAY:
                return tile.HasFlag(TILE_FLAG_FLIPPED_X) ? RULE_LEFT_ONLY : RULE_RIGHT_ONLY;
            case COLLISION_JUMP_DOWN:
                return RULE_NOT_UP;
            default:
                return RULE_BLOCKED;
        }
    }

    bool CanEnter(TileRule rule, Vec2i d) {
        switch (rule) {
            case RULE_OPEN:
                return true;
            case RULE_NOT_DOWN:
                return d.y <= 0;
            case RULE_NOT_UP:
                return d.y >= 0;
            case RULE_RIGHT_ONLY:
                return d.x > 0;
            case RULE_LEFT_ONLY:
                return d.x < 0;
            default:
                return false;
        }
    }

}

std::vector<Vec2i> FindPath(const World& world, const ItemDatabase& items, Vec2i start, Vec2i goal, int userID) {
    const int width = world.GetWidth();
    const int height = world.GetHeight();
    auto inside = [&](Vec2i p) { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; };
    auto index = [&](Vec2i p) { return p.y * width + p.x; };

    if (!inside(start) || !inside(goal))
        return {};

    std::vector<TileRule> rules(world.GetTiles().size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            rules[index({x, y})] = GetTileRule(world, items, x, y, userID);
    }

    // A* with Manhattan distance
    struct Node {
        int cost; // distance so far + estimate
        int distance;
        Vec2i pos;
        bool operator>(const Node& other) const { return cost > other.cost; }
    };
    auto estimate = [&](Vec2i p) { return std::abs(goal.x - p.x) + std::abs(goal.y - p.y); };

    std::vector<int> distance(rules.size(), INT32_MAX);
    std::vector<int> cameFrom(rules.size(), -1);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    distance[index(start)] = 0;
    open.push({estimate(start), 0, start});

    const Vec2i directions[] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
    while (!open.empty()) {
        Node current = open.top();
        open.pop();

        if (current.pos.x == goal.x && current.pos.y == goal.y) {
            std::vector<Vec2i> path;
            for (int i = index(goal); i != -1; i = cameFrom[i])
                path.push_back({i % width, i / width});
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (current.distance > distance[index(current.pos)])
            continue;

        for (Vec2i d : directions) {
            Vec2i next{current.pos.x + d.x, current.pos.y + d.y};
            if (!inside(next) || !CanEnter(rules[index(next)], d) || current.distance + 1 >= distance[index(next)])
                continue;

            distance[index(next)] = current.distance + 1;
            cameFrom[index(next)] = index(current.pos);
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

    Vec2i start;
    start.x = std::floor(state.local.pos.x / 32.0f);
    start.y = std::floor(state.local.pos.y / 32.0f);
    start.x = std::clamp(start.x, 0, world.GetWidth() - 1);
    start.y = std::clamp(start.y, 0, world.GetHeight() - 1);

    std::vector<Vec2i> path = FindPath(world, state.items, start, goal, state.local.userID);
    if (path.empty()) {
        proxy.TextOverlay("`4Path Not Found");
        return;
    }

    // tell the server we walked the path, one tile at a time
    size_t stride = path.size() < LONG_PATH ? 1 : 2;
    int delay = 0;
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
