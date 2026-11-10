// Dumps what the parsers extract from an items.dat and world files, in the same format as the reference dump
// made from the original code, so both can be diffed.
//
// Usage: ParserTests <items.dat> <output.txt> [world.bin ...]
#include "Core/Log.h"
#include "Game/Items/Items.h"
#include "Game/World/World.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

static std::vector<uint8> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: ParserTests <items.dat> <output.txt> [world.bin ...]\n");
        return 1;
    }

    Log::Init();

    ItemDatabase items;
    std::vector<uint8> itemData = ReadFile(argv[1]);
    if (!items.Load(itemData.data(), itemData.size()))
        return 2;

    FILE* out = std::fopen(argv[2], "w");
    std::fprintf(out, "ITEMS %u %zu\n", items.Version(), items.Count());
    for (size_t i = 0; i < items.Count(); ++i) {
        const Item* item = items.Get((int32)i);
        std::fprintf(out, "%d %u %u %u %d %u %u %s|%s\n", item->id, item->flags, item->type, item->collisionType,
                     item->rarity, item->maxAmount, item->growTime, item->name.c_str(), item->description.c_str());
    }

    for (int i = 3; i < argc; ++i) {
        std::vector<uint8> data = ReadFile(argv[i]);
        World world;
        bool ok = world.Load(data.data(), data.size(), items);
        std::fprintf(out, "WORLD %s ok=%d v=%u %dx%d\n", world.GetName().c_str(), ok, world.GetVersion(),
                     world.GetWidth(), world.GetHeight());
        if (!ok)
            continue;

        for (const Tile& tile : world.GetTiles()) {
            std::fprintf(out, "T %d %d %u %u %d %u\n", tile.foreground, tile.background, tile.parent, tile.flags,
                         tile.extraType, 0u);
        }

        std::fprintf(out, "OBJECTS %zu\n", world.GetDroppedItems().size());
        for (const DroppedItem& item : world.GetDroppedItems()) {
            std::fprintf(out, "O %d %u %u %u %.2f %.2f\n", item.itemID, item.count, item.flags, item.uid, item.pos.x,
                         item.pos.y);
        }
        std::fprintf(out, "WEATHER %u %u\n", world.GetBaseWeather(), world.GetWeather());
    }

    std::fclose(out);
    return 0;
}
