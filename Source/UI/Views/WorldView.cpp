#include "UI/ProxyUI.h"

#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/Widgets.h"
#include "Utils/Strings.h"

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <map>

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

    const char* CollisionName(uint8 type) {
        switch (type) {
            case COLLISION_NONE:
                return "None";
            case COLLISION_SOLID:
                return "Solid";
            case COLLISION_JUMP_THROUGH:
                return "Platform";
            case COLLISION_GATEWAY:
                return "Gateway";
            case COLLISION_IF_OFF:
                return "Solid when off";
            case COLLISION_ONE_WAY:
                return "One way";
            case COLLISION_VIP:
                return "VIP";
            case COLLISION_JUMP_DOWN:
                return "Solid from below";
            default:
                return "Unknown";
        }
    }

    // Foreground color in "collision" mode: what the tile does to movement, with locks/doors/seeds highlighted.
    ImVec4 CollisionColor(const Item* item) {
        if (!item)
            return Theme::Hex(0x4B5563);

        switch (item->type) {
            case ITEM_TYPE_LOCK:
                return Theme::Hex(0xF5C542);
            case ITEM_TYPE_SEED:
                return Theme::Hex(0x7BC86C);
            case ITEM_TYPE_DOOR:
            case ITEM_TYPE_USER_DOOR:
            case ITEM_TYPE_PORTAL:
            case ITEM_TYPE_SUNGATE:
                return Theme::Hex(0x38BDF8);
            case ITEM_TYPE_VIP_DOOR:
            case ITEM_TYPE_FRIENDS_ENTRANCE:
                return Theme::Hex(0xF472B6);
            default:
                break;
        }

        switch (item->collisionType) {
            case COLLISION_NONE:
                return Theme::Hex(0x3A4250, 0.75f);
            case COLLISION_SOLID:
                return Theme::Hex(0x7A879A);
            case COLLISION_JUMP_THROUGH:
                return Theme::Hex(0xC08A3E);
            case COLLISION_GATEWAY:
                return Theme::Hex(0x3FAE7C);
            case COLLISION_IF_OFF:
                return Theme::Hex(0x8D6CD9);
            case COLLISION_ONE_WAY:
                return Theme::Hex(0x4FA3D9);
            case COLLISION_VIP:
                return Theme::Hex(0xD95F8C);
            case COLLISION_JUMP_DOWN:
                return Theme::Hex(0xC0A040);
            default:
                return Theme::Hex(0x4B5563);
        }
    }

    // RENDER_DIRECT8: variant cell (index into the 8x6 block, row by row) for each 8-neighbour mask. Corners only
    // count when both sides next to them are joined too, which leaves 47 shapes. Values from Mori
    // (github.com/CLOEI/Mori), with masks 160 and 224 fixed to cells 46 and 6 (all 47 shapes checked against fire.rttex).
    // Mask bits: 1 top-left, 2 top, 4 top-right, 8 right, 16 bottom-right, 32 bottom, 64 bottom-left, 128 left.
    const std::array<uint8, 256>& Direct8Cells() {
        static const std::array<uint8, 256> cells = [] {
            const std::pair<int, int> shapes[] = {
                {0, 12},   {2, 11},   {8, 29},   {10, 43},  {14, 7},   {32, 10},  {34, 9},   {40, 45},  {42, 33},
                {46, 32},  {56, 5},   {58, 31},  {62, 3},   {128, 30}, {130, 44}, {131, 8},  {136, 28}, {138, 42},
                {139, 41}, {142, 40}, {143, 2},  {160, 46}, {162, 36}, {163, 35}, {168, 39}, {170, 27}, {171, 23},
                {174, 24}, {175, 18}, {184, 37}, {186, 26}, {187, 22}, {190, 19}, {191, 15}, {224, 6},  {226, 34},
                {227, 4},  {232, 38}, {234, 25}, {235, 20}, {238, 21}, {239, 16}, {248, 1},  {250, 17}, {251, 14},
                {254, 13}, {255, 0},
            };
            std::array<uint8, 256> table{};
            for (int mask = 0; mask < 256; ++mask) {
                int m = mask;
                if (!(m & 2) || !(m & 128))
                    m &= ~1;
                if (!(m & 2) || !(m & 8))
                    m &= ~4;
                if (!(m & 32) || !(m & 8))
                    m &= ~16;
                if (!(m & 32) || !(m & 128))
                    m &= ~64;
                table[mask] = 12; // no neighbours
                for (auto [shape, cell] : shapes) {
                    if (shape == m)
                        table[mask] = cell;
                }
            }
            return table;
        }();
        return cells;
    }

    // Cell in an 8x6 block of joined shapes, from a callback telling which of the 8 neighbours are joined.
    template <typename Joined> int Direct8Cell(Joined&& joined) {
        int mask = joined(-1, -1) | joined(0, -1) << 1 | joined(1, -1) << 2 | joined(1, 0) << 3 | joined(1, 1) << 4 |
                   joined(0, 1) << 5 | joined(-1, 1) << 6 | joined(-1, 0) << 7;
        return Direct8Cells()[mask];
    }

    bool AttachesToWalls(const Item& item) {
        return item.renderType == RENDER_ATTACH_TO_WALL_5 || item.renderType == RENDER_ATTACH_TO_WALL_4;
    }

    // The cell an item shows when nothing is next to it (as in the inventory); used for the fruit on trees.
    Vec2i IconCell(const Item& item) {
        int variant = 0;
        switch (item.renderType) {
            case RENDER_DIRECT8:
            case RENDER_DIRECT4:
                variant = 12;
                break;
            case RENDER_HORIZONTAL:
            case RENDER_VERTICAL:
            case RENDER_CAVE_PLATFORM:
            case RENDER_DIAGONAL:
            case RENDER_ATTACH_TO_WALL_4:
                variant = 3;
                break;
            case RENDER_ATTACH_TO_WALL_5:
                variant = 4;
                break;
            default:
                break;
        }
        Vec2i cell;
        cell.x = item.textureX + variant % 8;
        cell.y = item.textureY + variant / 8;
        return cell;
    }

    // The texture cell the game draws for an item on a tile. Items that join their neighbours (dirt, platforms,
    // pipes...) pick a variant next to their base cell depending on which neighbours hold the same item. The
    // variant orders were read off the texture sheets; the world's edge counts as a neighbour, so blocks run
    // into it without a border.
    Vec2i TextureCell(const World& world, const ItemDatabase& items, const Tile& tile, const Item& item, int x, int y,
                      bool background) {
        // 1 when the neighbour holds the same item on the same layer, so results can be shifted into masks
        auto same = [&](int dx, int dy) {
            const Tile* other = world.GetTile(x + dx, y + dy);
            return !other || (background ? other->background : other->foreground) == item.id ? 1 : 0;
        };
        // a solid block to hang on to (other wall-attached items don't count, so rows of spikes stay aligned)
        auto wall = [&](int dx, int dy) {
            const Tile* other = world.GetTile(x + dx, y + dy);
            const Item* block = other && other->foreground != 0 ? items.Get(other->foreground) : nullptr;
            return block && block->collisionType == COLLISION_SOLID && !AttachesToWalls(*block);
        };
        // pieces of a line: first, middle, last, alone
        auto line = [](bool before, bool after) { return before && after ? 1 : after ? 0 : before ? 2 : 3; };

        Vec2i cell;
        cell.x = item.textureX;
        cell.y = item.textureY;
        int variant = 0; // cells right of the base cell (or index into a block for DIRECT8 / DIRECT4)

        switch (item.renderType) {
            case RENDER_DIRECT8:
                variant = Direct8Cell(same);
                break;
            case RENDER_DIRECT4: {
                // mask bits: 1 top, 2 left, 4 right, 8 bottom (table from Mori, checked with ice)
                static const int cells[16] = {12, 11, 15, 8, 14, 7, 13, 2, 10, 9, 6, 4, 5, 3, 1, 0};
                int mask = same(0, -1) | same(-1, 0) << 1 | same(1, 0) << 2 | same(0, 1) << 3;
                variant = cells[mask];
                break;
            }
            case RENDER_HORIZONTAL:
                variant = line(same(-1, 0), same(1, 0));
                break;
            case RENDER_CAVE_PLATFORM: {
                bool left = same(-1, 0), right = same(1, 0);
                variant = !left && wall(-1, 0) ? 4 : !right && wall(1, 0) ? 5 : line(left, right);
                break;
            }
            case RENDER_VERTICAL:
                // bottom piece first: streetlamp base, pole, lamp
                variant = line(same(0, 1), same(0, -1));
                break;
            case RENDER_DIAGONAL: {
                // rises to the right, or to the left when flipped; lowest step first
                int rise = tile.HasFlag(TILE_FLAG_FLIPPED_X) ? -1 : 1;
                variant = line(same(-rise, 1), same(rise, -1));
                break;
            }
            case RENDER_ATTACH_TO_WALL_5:
            case RENDER_ATTACH_TO_WALL_4:
                // shortcut: the order the sides are tried in is a guess (floor first); the sheets only give the cells
                variant = wall(0, 1) ? 3 : wall(0, -1) ? 1 : wall(-1, 0) ? 0 : wall(1, 0) ? 2
                          : item.renderType == RENDER_ATTACH_TO_WALL_5 ? 4 : 3;
                break;
            case RENDER_RANDOM:
                variant = (unsigned)(x * 7919 + y * 104729) % 4; // fixed per tile, so the map doesn't flicker
                break;
            default:
                break;
        }
        cell.x += variant % 8;
        cell.y += variant / 8;

        // switched on (doors, switches, ...): the "on" picture is the next cell
        if (tile.HasFlag(background ? TILE_FLAG_BG_IS_ON : TILE_FLAG_IS_ON))
            cell.x += 1;
        return cell;
    }

    // Tint of a painted tile (paint bucket colors mix: red + green is yellow, all three is charcoal).
    ImU32 PaintColor(const Tile& tile) {
        bool red = tile.HasFlag(TILE_FLAG_PAINTED_RED);
        bool green = tile.HasFlag(TILE_FLAG_PAINTED_GREEN);
        bool blue = tile.HasFlag(TILE_FLAG_PAINTED_BLUE);
        if (!red && !green && !blue)
            return IM_COL32_WHITE;
        if (red && green && blue)
            return IM_COL32(70, 70, 70, 255);
        return IM_COL32(red ? 255 : 60, green ? 255 : 60, blue ? 255 : 60, 255);
    }

    // Item colors in items.dat are stored as B, G, R, A from the high byte down.
    ImU32 ItemColor(uint32 color) { return IM_COL32((color >> 8) & 0xFF, (color >> 16) & 0xFF, color >> 24, 255); }

    struct FlagName {
        uint16 flag;
        const char* name;
    };

    constexpr FlagName TILE_FLAGS[] = {
        {TILE_FLAG_HAS_PARENT, "Has parent"},   {TILE_FLAG_WAS_SPLICED, "Spliced"},
        {TILE_FLAG_WILL_SPAWN_SEEDS, "Seeds"},  {TILE_FLAG_IS_SEEDLING, "Seedling"},
        {TILE_FLAG_FLIPPED_X, "Flipped"},       {TILE_FLAG_IS_ON, "On"},
        {TILE_FLAG_IS_OPEN_TO_PUBLIC, "Public"}, {TILE_FLAG_BG_IS_ON, "Background on"},
        {TILE_FLAG_FG_ALT_MODE, "Alt mode"},    {TILE_FLAG_IS_WET, "Wet"},
        {TILE_FLAG_GLUED, "Glued"},             {TILE_FLAG_ON_FIRE, "On fire"},
        {TILE_FLAG_PAINTED_RED, "Red paint"},   {TILE_FLAG_PAINTED_GREEN, "Green paint"},
        {TILE_FLAG_PAINTED_BLUE, "Blue paint"},
    };

    // Selectable list row with a primary label and a muted line under it. Returns true when clicked.
    bool ListRow(const char* id, const char* title, const char* detail, float rightReserve) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float height = ImGui::GetTextLineHeight() * 2.0f + 10.0f * Scale();
        float width = ImGui::GetContentRegionAvail().x;

        ImGui::PushID(id);
        bool clicked = ImGui::InvisibleButton("##row", ImVec2(std::max(1.0f, width - rightReserve), height));
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (hovered)
            draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), Theme::U32(Theme::Hex(0xFFFFFF, 0.04f)),
                                7.0f * Scale());
        float x = pos.x + 8.0f * Scale();
        draw->AddText(ImVec2(x, pos.y + 5.0f * Scale()), Theme::U32(Theme::Text), title);
        draw->AddText(ImVec2(x, pos.y + 5.0f * Scale() + ImGui::GetTextLineHeight()), Theme::U32(Theme::TextFaint),
                      detail);

        // leave the cursor at the row's right edge for an action button on the same line
        ImGui::SameLine(0, 0);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - rightReserve, pos.y + (height - ImGui::GetFrameHeight()) * 0.5f));
        return clicked;
    }

}

void ProxyUI::DrawWorld() {
    const World& world = m_proxy.State().world;
    if (!world.IsLoaded()) {
        Widgets::EmptyState(ICON_FA_EARTH_AMERICAS, "Not in a world", "Join a world in the game and it shows up here.");
        return;
    }

    if (m_world.worldName != world.GetName()) {
        m_world.worldName = world.GetName();
        m_world.needsFit = true;
        m_world.selectedX = m_world.selectedY = -1;
    }

    static const char* const TABS[] = {ICON_FA_CIRCLE_INFO "  Overview", ICON_FA_MAP "  Map"};
    Widgets::Segmented("##world_tabs", &m_world.tab, TABS, 2);
    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

    if (m_world.tab == 0) {
        DrawWorldInspector(ImGui::GetContentRegionAvail());
    } else {
        DrawWorldToolbar();
        ImGui::Dummy(ImVec2(0, 2.0f * Scale()));
        DrawWorldMap(ImGui::GetContentRegionAvail());
    }
}

void ProxyUI::DrawWorldToolbar() {
    const float spacing = 16.0f * Scale();
    Widgets::Switch("Textures", &m_world.showTextures);
    Widgets::Tooltip("Draws blocks with the game's textures; off shows them as colors by collision type");
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Background"), spacing);
    Widgets::Switch("Background", &m_world.showBackground);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Players"), spacing);
    Widgets::Switch("Players", &m_world.showPlayers);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Floating"), spacing);
    Widgets::Switch("Floating", &m_world.showDropped);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("No access"), spacing);
    Widgets::Switch("No access", &m_world.showAccess);
    Widgets::Tooltip("Tints tiles protected by a lock you don't have access to");
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Tile info"), spacing);
    Widgets::Switch("Tile info", &m_world.showTileInfo);
    Widgets::Tooltip("Shows a tile's details while the mouse is over it");

    // zoom controls on the right
    char zoom[16];
    snprintf(zoom, sizeof(zoom), "%d%%", (int)std::round(m_world.zoom * 100.0f));
    float buttons = ImGui::GetFrameHeight() * 3.0f + ImGui::CalcTextSize("1600%").x + 30.0f * Scale();
    Widgets::SameLineIfFits(buttons);
    Widgets::AlignRight(buttons);
    if (Widgets::IconButton("##zoomout", ICON_FA_MAGNIFYING_GLASS_MINUS, "Zoom out"))
        m_world.zoom = std::max(1.0f, m_world.zoom / 1.25f);
    ImGui::SameLine(0, 2.0f * Scale());
    ImGui::AlignTextToFramePadding();
    Widgets::TextColored(Theme::TextMuted, Theme::FONT_BODY, "%s", zoom);
    ImGui::SameLine(0, 2.0f * Scale());
    if (Widgets::IconButton("##zoomin", ICON_FA_MAGNIFYING_GLASS_PLUS, "Zoom in"))
        m_world.zoom = std::min(16.0f, m_world.zoom * 1.25f);
    ImGui::SameLine(0, 2.0f * Scale());
    if (Widgets::IconButton("##fit", ICON_FA_EXPAND, "Fit to view"))
        m_world.needsFit = true;
}

void ProxyUI::DrawWorldMap(ImVec2 size) {
    GameState& state = m_proxy.State();
    const World& world = state.world;
    const ItemDatabase& items = state.items;
    ImGuiIO& io = ImGui::GetIO();

    size.x = std::max(size.x, 64.0f);
    size.y = std::max(size.y, 64.0f);
    const float width = (float)world.GetWidth();
    const float height = (float)world.GetHeight();
    if (width <= 0 || height <= 0)
        return;

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##map", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (ImGui::IsItemActivated())
        m_world.dragging = false;

    // tile size: "fit" shows the whole world, zoom multiplies it
    const float margin = 16.0f * Scale();
    const float fit = std::max(1.0f, std::min((size.x - margin * 2.0f) / width, (size.y - margin * 2.0f) / height));
    float tile = fit * m_world.zoom;

    // the tile size changed since last frame (zoom buttons, window resize): keep the view's center in place
    float lastTile = m_world.lastTile;
    if (lastTile > 0.0f && std::fabs(lastTile - tile) > 0.001f) {
        ImVec2 center(size.x * 0.5f, size.y * 0.5f);
        ImVec2 point((center.x - m_world.offset.x) / lastTile, (center.y - m_world.offset.y) / lastTile);
        m_world.offset = ImVec2(center.x - point.x * tile, center.y - point.y * tile);
    }

    if (m_world.needsFit) {
        m_world.zoom = 1.0f;
        tile = fit;
        m_world.offset = ImVec2((size.x - width * tile) * 0.5f, (size.y - height * tile) * 0.5f);
        m_world.needsFit = false;
    }

    // zoom around the mouse
    ImVec2 mouse(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    if (hovered && io.MouseWheel != 0.0f) {
        float before = tile;
        m_world.zoom = std::clamp(m_world.zoom * std::pow(1.2f, io.MouseWheel), 1.0f, 16.0f);
        tile = fit * m_world.zoom;
        ImVec2 point((mouse.x - m_world.offset.x) / before, (mouse.y - m_world.offset.y) / before);
        m_world.offset = ImVec2(mouse.x - point.x * tile, mouse.y - point.y * tile);
    }
    m_world.lastTile = tile;

    // pan with left or middle drag
    if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))) {
        m_world.offset.x += io.MouseDelta.x;
        m_world.offset.y += io.MouseDelta.y;
        m_world.dragging = true;
    }
    if (active && m_world.dragging)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    // tile under the mouse
    m_world.hoverX = m_world.hoverY = -1;
    if (hovered) {
        int32 x = (int32)std::floor((mouse.x - m_world.offset.x) / tile);
        int32 y = (int32)std::floor((mouse.y - m_world.offset.y) / tile);
        if (x >= 0 && y >= 0 && x < world.GetWidth() && y < world.GetHeight()) {
            m_world.hoverX = x;
            m_world.hoverY = y;
        }
    }

    bool hasHover = m_world.hoverX >= 0;
    if (ImGui::IsItemDeactivated() && !m_world.dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && hasHover) {
        m_world.selectedX = m_world.hoverX;
        m_world.selectedY = m_world.hoverY;
    }
    if (hovered && hasHover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        WalkTo(m_world.hoverX, m_world.hoverY);

    static int32 s_contextX = -1, s_contextY = -1;
    if (hovered && hasHover && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        s_contextX = m_world.hoverX;
        s_contextY = m_world.hoverY;
        ImGui::OpenPopup("##tile_menu");
    }

    // drawing
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 end(origin.x + size.x, origin.y + size.y);
    draw->AddRectFilled(origin, end, Theme::U32(Theme::Hex(0x090B0E)), Theme::CARD_ROUNDING);
    draw->PushClipRect(origin, end, true);

    ImVec2 base(origin.x + m_world.offset.x, origin.y + m_world.offset.y);
    draw->AddRectFilled(base, ImVec2(base.x + width * tile, base.y + height * tile), Theme::U32(Theme::Hex(0x0F1823)));

    int32 x0 = std::max(0, (int32)std::floor(-m_world.offset.x / tile));
    int32 y0 = std::max(0, (int32)std::floor(-m_world.offset.y / tile));
    int32 x1 = std::min(world.GetWidth() - 1, (int32)std::floor((size.x - m_world.offset.x) / tile));
    int32 y1 = std::min(world.GetHeight() - 1, (int32)std::floor((size.y - m_world.offset.y) / tile));

    const float inset = tile >= 6.0f ? std::max(0.5f, tile * 0.06f) : 0.0f;
    const float rounding = tile >= 12.0f ? tile * 0.14f : 0.0f;
    const ImU32 backgroundColor = Theme::U32(Theme::Hex(0x1B2330));
    const ImU32 noAccess = Theme::U32(Theme::WithAlpha(Theme::Danger, 0.22f));
    const ImU32 lockOutline = Theme::U32(Theme::Hex(0xF5C542, 0.9f));

    // draws a sheet cell over the tile, mirrored for flipped tiles and multiplied by tint
    auto drawSprite = [&](const string& sheet, Vec2i cell, bool flipped, ImVec2 min, ImVec2 max,
                          ImU32 tint = IM_COL32_WHITE) {
        TextureCache::Sprite sprite;
        if (!m_textures.GetSprite(sheet, cell.x, cell.y, sprite))
            return false;
        if (flipped)
            std::swap(sprite.uv0.x, sprite.uv1.x);
        draw->AddImage(sprite.texture, min, max, sprite.uv0, sprite.uv1, tint);
        return true;
    };

    // planted seeds are drawn as their tree: trunk and leaves, both grey in the sheet and tinted by the seed. The
    // tree grows from the ground up to the full tile, then shows its fruit.
    const uint32 now = (uint32)std::time(nullptr);
    auto drawTree = [&](const Tile& t, const Item& seed, ImVec2 min, ImVec2 max) {
        float growth = 1.0f; // fully grown when the age is unknown
        if (seed.growTime > 0 && t.plantedAt != 0)
            growth = now > t.plantedAt ? std::min(1.0f, (float)(now - t.plantedAt) / (float)seed.growTime) : 0.0f;

        float size = tile * (0.35f + 0.65f * growth);
        ImVec2 treeMin((min.x + max.x - size) * 0.5f, max.y - size);
        ImVec2 treeMax(treeMin.x + size, max.y);
        bool flipped = t.HasFlag(TILE_FLAG_FLIPPED_X);

        Vec2i trunk, leaves;
        trunk.x = seed.treeBase;
        trunk.y = 19;
        leaves.x = seed.treeLeaves;
        leaves.y = 18;
        if (!drawSprite("tiles_page1.rttex", trunk, flipped, treeMin, treeMax, ItemColor(seed.treeColor)))
            return false;
        drawSprite("tiles_page1.rttex", leaves, flipped, treeMin, treeMax, ItemColor(seed.leavesColor));

        // ready to harvest: the fruit hangs in the leaves
        const Item* fruit = growth >= 1.0f && tile >= 10.0f ? items.Get(seed.id - 1) : nullptr;
        if (fruit) {
            static const ImVec2 spots[] = {{0.16f, 0.14f}, {0.52f, 0.10f}, {0.34f, 0.34f}, {0.04f, 0.38f}, {0.64f, 0.36f}};
            const float fruitSize = tile * 0.32f;
            for (int i = 0; i < std::min<int>(t.fruits, IM_ARRAYSIZE(spots)); ++i) {
                ImVec2 at(min.x + spots[i].x * tile, min.y + spots[i].y * tile);
                drawSprite(fruit->texture, IconCell(*fruit), false, at, ImVec2(at.x + fruitSize, at.y + fruitSize));
            }
        }
        return true;
    };

    // draws the tile's item with its texture; false when it has none, so the caller falls back to a color
    auto drawTexture = [&](const Tile& t, int32 x, int32 y, bool background, ImVec2 min, ImVec2 max) {
        const Item* item = items.Get(background ? t.background : t.foreground);
        if (!m_world.showTextures || !item)
            return false;
        if (item->type == ITEM_TYPE_SEED && !background && drawTree(t, *item, min, max))
            return true;

        // paint colors the foreground, or the background of a tile without one
        ImU32 tint = background == (t.foreground == 0) ? PaintColor(t) : IM_COL32_WHITE;
        return drawSprite(item->texture, TextureCell(world, items, t, *item, x, y, background),
                          t.HasFlag(TILE_FLAG_FLIPPED_X), min, max, tint);
    };

    // water and fire spread over tiles like a block that joins its neighbours
    auto drawOverlay = [&](const char* sheet, uint16 flag, int32 x, int32 y, ImVec2 min, ImVec2 max, ImU32 tint) {
        int variant = Direct8Cell([&](int dx, int dy) {
            const Tile* other = world.GetTile(x + dx, y + dy);
            return !other || other->HasFlag(flag) ? 1 : 0;
        });
        Vec2i cell;
        cell.x = variant % 8;
        cell.y = variant / 8;
        drawSprite(sheet, cell, false, min, max, tint);
    };

    for (int32 y = y0; y <= y1; ++y) {
        for (int32 x = x0; x <= x1; ++x) {
            const Tile* t = world.GetTile(x, y);
            if (!t)
                continue;

            // whole pixels, each tile ending exactly where the next one starts, so no gap can open between them
            ImVec2 min(std::floor(base.x + x * tile), std::floor(base.y + y * tile));
            ImVec2 max(std::floor(base.x + (x + 1) * tile), std::floor(base.y + (y + 1) * tile));
            if (m_world.showBackground && t->background != 0 && !drawTexture(*t, x, y, true, min, max))
                draw->AddRectFilled(min, max, backgroundColor);

            if (t->foreground != 0 && !drawTexture(*t, x, y, false, min, max)) {
                const Item* item = items.Get(t->foreground);
                ImVec4 color = CollisionColor(item);
                draw->AddRectFilled(ImVec2(min.x + inset, min.y + inset), ImVec2(max.x - inset, max.y - inset),
                                    Theme::U32(color), rounding);
                if (item && item->type == ITEM_TYPE_LOCK && tile >= 5.0f)
                    draw->AddRect(min, max, lockOutline, rounding, 0, std::max(1.0f, tile * 0.08f));
            }

            if (m_world.showTextures) {
                // the fire sheet is a white shape with a flame-colored rim; tinted, it reads as a glow
                if (t->HasFlag(TILE_FLAG_ON_FIRE))
                    drawOverlay("fire.rttex", TILE_FLAG_ON_FIRE, x, y, min, max, IM_COL32(255, 150, 40, 170));
                if (t->HasFlag(TILE_FLAG_IS_WET))
                    drawOverlay("water.rttex", TILE_FLAG_IS_WET, x, y, min, max, IM_COL32(255, 255, 255, 150));
            }

            if (m_world.showAccess && state.local.userID != -1 && !world.HasAccess(x, y, state.local.userID))
                draw->AddRectFilled(min, max, noAccess);
        }
    }

    // floating items
    if (m_world.showDropped) {
        const float r = std::max(2.5f, tile * 0.2f);
        const ImU32 fill = Theme::U32(Theme::Warning);
        const ImU32 outline = Theme::U32(Theme::Hex(0x000000, 0.6f));
        for (const DroppedItem& dropped : world.GetDroppedItems()) {
            ImVec2 c(base.x + (dropped.pos.x + 8.0f) / 32.0f * tile, base.y + (dropped.pos.y + 8.0f) / 32.0f * tile);
            ImVec2 p[4] = {{c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}};
            draw->AddQuadFilled(p[0], p[1], p[2], p[3], fill);
            draw->AddQuad(p[0], p[1], p[2], p[3], outline, 1.0f);
        }
    }

    // players
    if (m_world.showPlayers) {
        const float r = std::max(3.5f, tile * 0.34f);
        for (const auto& [netID, player] : state.players) {
            if (netID == state.local.netID)
                continue;
            ImVec2 c(base.x + (player.tile.x + 0.5f) * tile, base.y + (player.tile.y + 0.5f) * tile);
            draw->AddCircleFilled(c, r + 1.5f, Theme::U32(Theme::Hex(0x000000, 0.6f)));
            draw->AddCircleFilled(c, r, Theme::U32(Theme::Info));

            bool onHover = player.tile.x == m_world.hoverX && player.tile.y == m_world.hoverY;
            if ((tile >= 14.0f || onHover) && !player.name.empty()) {
                ImVec2 text = ImGui::CalcTextSize(player.name.c_str());
                ImVec2 at(c.x - text.x * 0.5f, c.y - r - text.y - 4.0f);
                draw->AddRectFilled(ImVec2(at.x - 5, at.y - 1), ImVec2(at.x + text.x + 5, at.y + text.y + 1),
                                    Theme::U32(Theme::Hex(0x000000, 0.65f)), 4.0f);
                draw->AddText(at, Theme::U32(Theme::Text), player.name.c_str());
            }
        }

        if (state.local.netID != -1) {
            ImVec2 c(base.x + (state.local.pos.x / 32.0f + 0.5f) * tile, base.y + (state.local.pos.y / 32.0f + 0.5f) * tile);
            float t = (float)std::fmod(ImGui::GetTime(), 1.8) / 1.8f;
            draw->AddCircleFilled(c, r * (1.3f + t * 1.6f), Theme::U32(Theme::WithAlpha(Theme::Accent, 0.30f * (1.0f - t))));
            draw->AddCircleFilled(c, r + 1.5f, Theme::U32(Theme::Hex(0x000000, 0.6f)));
            draw->AddCircleFilled(c, r, Theme::U32(Theme::Accent));
        }
    }

    // hover and selection outlines
    auto outline = [&](int32 x, int32 y, ImU32 color, float thickness) {
        ImVec2 min(base.x + x * tile, base.y + y * tile);
        draw->AddRect(min, ImVec2(min.x + tile, min.y + tile), color, rounding, 0, thickness);
    };
    if (hasHover && !m_world.dragging)
        outline(m_world.hoverX, m_world.hoverY, Theme::U32(Theme::Hex(0xFFFFFF, 0.8f)), 1.5f);
    if (m_world.selectedX >= 0)
        outline(m_world.selectedX, m_world.selectedY, Theme::U32(Theme::Accent), 2.0f);

    // hint in the bottom-left corner
    const char* hint = "Scroll to zoom  \xC2\xB7  Drag to pan  \xC2\xB7  Click to select\xC2\xB7  Double-click to walk";
    Theme::PushFontSize(Theme::FONT_SMALL);
    ImVec2 hintSize = ImGui::CalcTextSize(hint);
    ImVec2 hintPos(origin.x + 12.0f * Scale(), end.y - hintSize.y - 12.0f * Scale());
    draw->AddRectFilled(ImVec2(hintPos.x - 8, hintPos.y - 4), ImVec2(hintPos.x + hintSize.x + 8, hintPos.y + hintSize.y + 4),
                        Theme::U32(Theme::Hex(0x0B0D10, 0.85f)), 6.0f);
    draw->AddText(hintPos, Theme::U32(Theme::TextMuted), hint);
    ImGui::PopFont();

    draw->PopClipRect();
    draw->AddRect(origin, end, Theme::U32(Theme::Border), Theme::CARD_ROUNDING);

    // hover tooltip
    if (m_world.showTileInfo && hasHover && !active) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * Scale(), 10.0f * Scale()));
        ImGui::BeginTooltip();
        DrawTileTooltip(m_world.hoverX, m_world.hoverY);
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }

    // right-click menu
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * Scale(), 8.0f * Scale()));
    if (ImGui::BeginPopup("##tile_menu")) {
        Widgets::TextColored(Theme::TextMuted, Theme::FONT_SMALL, "Tile %d, %d", s_contextX, s_contextY);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_PERSON_WALKING "  Walk here"))
            WalkTo(s_contextX, s_contextY);
        if (ImGui::MenuItem(ICON_FA_COPY "  Copy coordinates")) {
            char text[32];
            snprintf(text, sizeof(text), "%d, %d", s_contextX, s_contextY);
            ImGui::SetClipboardText(text);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void ProxyUI::DrawWorldInspector(ImVec2 size) {
    Widgets::BeginCard("##inspector", size);

    static const char* const TABS[] = {"Players", "Floating", "Blocks"};
    Widgets::Segmented("##inspector_tabs", &m_world.inspectorTab, TABS, 3);
    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

    if (m_world.inspectorTab >= 1) {
        Widgets::SearchBox("##world_search", m_world.search, sizeof(m_world.search), "Filter items");
        ImGui::Dummy(ImVec2(0, 2.0f * Scale()));
    }

    ImGui::BeginChild("##inspector_body", ImVec2(0, 0));
    switch (m_world.inspectorTab) {
        case 0:
            DrawPlayerList();
            break;
        case 1:
            DrawFloatingList();
            break;
        default:
            DrawBlockList();
            break;
    }
    ImGui::EndChild();

    Widgets::EndCard();
}

// Contents of the map's hover tooltip: what is on the tile, its flags and who may build there.
void ProxyUI::DrawTileTooltip(int32 x, int32 y) {
    const GameState& state = m_proxy.State();
    const World& world = state.world;
    const Tile* tile = world.GetTile(x, y);
    if (!tile)
        return;

    Widgets::TextColored(Theme::Text, Theme::FONT_LARGE, "Tile %d, %d", x, y);

    auto itemBlock = [&](const char* label, uint16 id) {
        ImGui::Dummy(ImVec2(0, 2.0f * Scale()));
        Widgets::SectionLabel(label);
        if (id == 0) {
            Widgets::TextColored(Theme::TextFaint, Theme::FONT_BODY, "Empty");
            return;
        }
        ImGui::TextUnformatted(state.items.GetName(id).c_str());
        if (const Item* item = state.items.Get(id))
            Widgets::TextColored(Theme::TextMuted, Theme::FONT_SMALL, "ID %u  \xC2\xB7  %s", id,
                                 CollisionName(item->collisionType));
        else
            Widgets::TextColored(Theme::TextMuted, Theme::FONT_SMALL, "ID %u", id);
    };
    itemBlock("Foreground", tile->foreground);
    const Item* seed = state.items.Get(tile->foreground);
    if (seed && seed->type == ITEM_TYPE_SEED && tile->foreground != 0 && tile->plantedAt != 0) {
        uint32 age = (uint32)std::max<int64>(0, (int64)std::time(nullptr) - tile->plantedAt);
        if (age >= seed->growTime)
            Widgets::TextColored(Theme::Accent, Theme::FONT_SMALL, "Ready to harvest  \xC2\xB7  %u fruit", tile->fruits);
        else
            Widgets::TextColored(Theme::TextMuted, Theme::FONT_SMALL, "%u%% grown  \xC2\xB7  %u fruit",
                                 (uint32)((uint64)age * 100 / seed->growTime), tile->fruits);
    }
    itemBlock("Background", tile->background);

    string flags;
    for (const FlagName& flag : TILE_FLAGS) {
        if (tile->HasFlag(flag.flag))
            flags += (flags.empty() ? "" : ", ") + string(flag.name);
    }
    if (tile->extraType != 0)
        flags += (flags.empty() ? "" : ", ") + ("Extra data #" + std::to_string(tile->extraType));
    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));
    Widgets::SectionLabel("Flags");
    if (flags.empty())
        Widgets::TextColored(Theme::TextFaint, Theme::FONT_BODY, "None");
    else
        ImGui::TextUnformatted(flags.c_str());

    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));
    Widgets::SectionLabel("Access");
    int32 userID = state.local.userID;
    bool access = userID == -1 || world.HasAccess(x, y, userID);
    Widgets::TextColored(access ? Theme::Accent : Theme::Danger, Theme::FONT_BODY,
                         access ? ICON_FA_CIRCLE_CHECK "  You can build here" : ICON_FA_LOCK "  Protected");
    if (const TileAccess* own = world.GetAccess(x, y))
        Widgets::TextColored(Theme::TextMuted, Theme::FONT_SMALL, "Owner %d  \xC2\xB7  %zu allowed", own->owner,
                             own->allowed.size());

    int32 here = 0;
    for (const DroppedItem& dropped : world.GetDroppedItems()) {
        if ((int32)((dropped.pos.x + 8.0f) / 32.0f) == x && (int32)((dropped.pos.y + 8.0f) / 32.0f) == y)
            here += dropped.count;
    }
    if (here > 0) {
        ImGui::Dummy(ImVec2(0, 2.0f * Scale()));
        Widgets::SectionLabel("Floating here");
        ImGui::Text("%d items", here);
    }
}

void ProxyUI::DrawPlayerList() {
    const GameState& state = m_proxy.State();
    if (state.players.empty()) {
        Widgets::EmptyState(ICON_FA_USERS, "No players", "Nobody has spawned in this world yet.");
        return;
    }

    // local player first, then by name
    std::vector<const Player*> players;
    for (const auto& [netID, player] : state.players)
        players.push_back(&player);
    std::sort(players.begin(), players.end(), [&](const Player* a, const Player* b) {
        bool la = a->netID == state.local.netID, lb = b->netID == state.local.netID;
        if (la != lb)
            return la;
        return Strings::ToLower(a->name) < Strings::ToLower(b->name);
    });

    float reserve = ImGui::GetFrameHeight() + 4.0f * Scale();
    for (const Player* player : players) {
        bool local = player->netID == state.local.netID;
        Vec2i tile = local ? Vec2i{(int32)std::floor(state.local.pos.x / 32.0f), (int32)std::floor(state.local.pos.y / 32.0f)}
                           : player->tile;

        char id[24], title[96], detail[96];
        snprintf(id, sizeof(id), "player%d", player->netID);
        snprintf(title, sizeof(title), "%s%s", player->name.empty() ? "(unnamed)" : player->name.c_str(), local ? "  (you)" : "");
        snprintf(detail, sizeof(detail), "net %d  \xC2\xB7  user %d  \xC2\xB7  tile %d, %d", player->netID, player->userID,
                 tile.x, tile.y);

        if (ListRow(id, title, detail, reserve)) {
            m_world.selectedX = tile.x;
            m_world.selectedY = tile.y;
        }
        if (!local) {
            if (Widgets::IconButton(id, ICON_FA_PERSON_WALKING, "Walk to this player"))
                WalkTo(tile.x, tile.y);
        } else {
            ImGui::Dummy(ImVec2(reserve, ImGui::GetFrameHeight()));
        }
    }
}

void ProxyUI::DrawFloatingList() {
    const GameState& state = m_proxy.State();

    struct Entry {
        uint32 count = 0;
        uint32 stacks = 0;
    };
    std::map<uint16, Entry> totals;
    for (const DroppedItem& dropped : state.world.GetDroppedItems()) {
        if (dropped.itemID == 0 || dropped.count == 0)
            continue;
        Entry& entry = totals[dropped.itemID];
        entry.count += dropped.count;
        entry.stacks++;
    }

    if (totals.empty()) {
        Widgets::EmptyState(ICON_FA_GEM, "No floating items", "Dropped items in this world are listed here.");
        return;
    }

    std::vector<std::pair<uint16, Entry>> rows(totals.begin(), totals.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second.count > b.second.count; });

    string filter = Strings::ToLower(m_world.search);
    float reserve = ImGui::GetFrameHeight() + 4.0f * Scale();
    bool any = false;
    for (const auto& [itemID, entry] : rows) {
        string name = state.items.GetName(itemID);
        if (!filter.empty() && Strings::ToLower(name).find(filter) == string::npos)
            continue;
        any = true;

        char id[24], title[128], detail[64];
        snprintf(id, sizeof(id), "float%u", itemID);
        snprintf(title, sizeof(title), "%s  \xC3\x97%u", name.c_str(), entry.count);
        snprintf(detail, sizeof(detail), "ID %u  \xC2\xB7  %u stack%s", itemID, entry.stacks, entry.stacks == 1 ? "" : "s");
        ListRow(id, title, detail, reserve);
        ImGui::BeginDisabled(!m_proxy.IsClientConnected());
        if (Widgets::IconButton(id, ICON_FA_LOCATION_CROSSHAIRS, "Point the in-game arrow at this item", Theme::Warning))
            m_proxy.GetCommands().PointToItem(itemID);
        ImGui::EndDisabled();
    }
    if (!any)
        Widgets::TextColored(Theme::TextFaint, Theme::FONT_BODY, "No items match the filter.");
}

void ProxyUI::DrawBlockList() {
    const GameState& state = m_proxy.State();

    std::map<uint16, uint32> counts;
    for (const Tile& tile : state.world.GetTiles()) {
        if (tile.foreground != 0)
            counts[tile.foreground]++;
        if (tile.background != 0)
            counts[tile.background]++;
    }
    if (counts.empty()) {
        Widgets::EmptyState(ICON_FA_CUBE, "No blocks", "This world is empty.");
        return;
    }

    std::vector<std::pair<uint16, uint32>> rows(counts.begin(), counts.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    string filter = Strings::ToLower(m_world.search);
    if (ImGui::BeginTable("##blocks", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 90.0f * Scale());
        for (const auto& [itemID, count] : rows) {
            string name = state.items.GetName(itemID);
            if (!filter.empty() && Strings::ToLower(name).find(filter) == string::npos)
                continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", Widgets::FormatNumber(count).c_str());
        }
        ImGui::EndTable();
    }
}
