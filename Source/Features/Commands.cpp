#include "Features/Commands.h"

#include "Proxy/Proxy.h"
#include "Packets/TextPacket.h"
#include "Utils/Strings.h"

#include <map>

namespace {

    // The antigravity generator is placed (client side only) on a fixed tile.
    constexpr int32 GENERATOR_X = 99;
    constexpr int32 GENERATOR_Y = 59;
    constexpr int32 ITEM_ANTIGRAVITY_GENERATOR = 4992;
    constexpr int32 ITEM_BEDROCK = 8;
    constexpr int32 ITEM_FIST = 18;

    GameUpdatePacket GeneratorTileChange(int32 itemID) {
        GameUpdatePacket packet;
        packet.type = PACKET_TILE_CHANGE_REQUEST;
        packet.value = itemID;
        packet.tileX = GENERATOR_X;
        packet.tileY = GENERATOR_Y;
        return packet;
    }

    // Punching the generator switches it on or off.
    GameUpdatePacket GeneratorPunch(const LocalPlayer& local) {
        GameUpdatePacket packet;
        packet.type = PACKET_TILE_APPLY_DAMAGE;
        packet.netID = local.netID;
        packet.value = 6;
        packet.pos = local.pos;
        packet.tileX = GENERATOR_X;
        packet.tileY = GENERATOR_Y;
        return packet;
    }

    const char* DIALOG_END = "\nadd_spacer|small|\nadd_quick_exit|";

}

bool Commands::HandleChat(string_view text) {
    text = Strings::Trim(text);
    if (text.empty() || text[0] != '/')
        return false;

    // "/name arguments"
    size_t space = text.find(' ');
    string name = Strings::ToLower(text.substr(1, space == string_view::npos ? string_view::npos : space - 1));
    string_view args = space == string_view::npos ? string_view() : Strings::Trim(text.substr(space + 1));

    if (name == "warp")
        return Warp(args);

    if (name == "growscan" || name == "gscan" || name == "gs") {
        m_proxy.ConsoleMessage("`6" + string(text));
        Growscan();
        return true;
    }

    if (name == "antigravity" || name == "ag") {
        ToggleAntigravity();
        return true;
    }

    return false;
}

bool Commands::Warp(string_view world) {
    if (world.empty())
        return false;

    m_proxy.Send(Server, MESSAGE_GAME_MESSAGE, "action|join_request\nname|" + string(world) + "\ninvitedWorld|0\n");
    m_proxy.ConsoleMessage("`9Warping to `#" + string(world));
    return true;
}

void Commands::Growscan() {
    m_proxy.ShowDialog("set_default_color|`o"
                       "\nadd_label_with_icon|big|`9Growscan|left|6016|"
                       "\nadd_spacer|small|"
                       "\nadd_button|gscan_float|`9Floating items``|noflags|0|0|"
                       "\nadd_button|gscan_block|`9World blocks``|noflags|0|0|" +
                       string(DIALOG_END) + "\nend_dialog|growscan_dialog|Close||");
}

void Commands::ToggleAntigravity() {
    GameState& state = m_proxy.State();

    if (state.antigravityEnabled) {
        state.antigravityEnabled = false;
        m_proxy.Send(Client, GeneratorPunch(state.local));
        m_proxy.Send(Client, GeneratorTileChange(ITEM_BEDROCK)); // put the original tile back
        m_proxy.ConsoleMessage("`4[PROXY] `oAntigravity Deactivated!");
        return;
    }

    state.antigravityEnabled = true;
    m_proxy.ConsoleMessage("`2[PROXY] Activating Antigravity...");
    m_proxy.Send(Client, GeneratorTileChange(ITEM_ANTIGRAVITY_GENERATOR));

    // give the client a moment to place the generator, then switch it on
    m_proxy.Schedule(1000, [this]() {
        const LocalPlayer& local = m_proxy.State().local;
        m_proxy.Send(Client, GeneratorPunch(local));

        GameUpdatePacket punchState;
        punchState.type = PACKET_STATE;
        punchState.value = ITEM_FIST;
        punchState.pos = local.pos;
        punchState.tileX = GENERATOR_X;
        punchState.tileY = GENERATOR_Y;
        punchState.flags = 2592;
        m_proxy.Send(Client, punchState);

        m_proxy.ConsoleMessage("`2[PROXY] `oAntigravity Activated!");
    });
}

bool Commands::HandleDialogReturn(const TextPacket& packet) {
    string_view button = packet.Get("buttonClicked");

    if (button == "gscan_block") {
        ShowWorldBlocks();
        return true;
    }
    if (button == "gscan_float") {
        ShowDroppedItems();
        return true;
    }

    // item buttons in the floating items list: point the arrow at the item
    for (string_view prefix : {string_view("floating_"), string_view("findObject_")}) {
        if (Strings::StartsWith(button, prefix)) {
            int32 itemID = Strings::ToInt(button.substr(prefix.size())).value_or(0);
            if (itemID > 0) {
                GameUpdatePacket arrow;
                arrow.type = PACKET_ACTIVE_ARROW_TO_ITEM;
                arrow.byte1 = 1;
                arrow.netID = itemID;
                m_proxy.Send(Client, arrow);
            }
            return true;
        }
    }

    return Strings::StartsWith(packet.Get("dialog_name"), "growscan");
}

void Commands::ShowWorldBlocks() {
    GameState& state = m_proxy.State();
    if (!state.world.IsLoaded()) {
        m_proxy.ConsoleMessage("`4You must be in a world to scan world blocks!");
        return;
    }

    std::map<uint16, uint32> counts;
    for (const Tile& tile : state.world.GetTiles()) {
        if (tile.foreground != 0)
            counts[tile.foreground]++;
        if (tile.background != 0)
            counts[tile.background]++;
    }

    string list;
    for (const auto& [id, count] : counts) {
        list += "\nadd_label_with_icon|small|`0" + state.items.GetName(id) + " : `w" + std::to_string(count) +
                "``|left|" + std::to_string(id) + "|";
    }
    if (counts.empty())
        list = "\nadd_textbox|`4No blocks found in this world.|left|2480|";

    m_proxy.ShowDialog("set_default_color|`o"
                       "\nadd_label_with_icon|big|`0World blocks: `0``|left|170|"
                       "\nadd_spacer|small|" +
                       list + DIALOG_END + "\nend_dialog|growscan_dialog|Close||");
}

void Commands::ShowDroppedItems() {
    GameState& state = m_proxy.State();
    if (!state.world.IsLoaded()) {
        m_proxy.ConsoleMessage("`4You must be in a world to scan floating items!");
        return;
    }

    std::map<uint16, uint32> counts;
    for (const DroppedItem& item : state.world.GetDroppedItems()) {
        if (item.itemID != 0 && item.count != 0)
            counts[item.itemID] += item.count;
    }

    string list;
    for (const auto& [id, count] : counts) {
        string idText = std::to_string(id);
        list += "\nadd_label_with_icon_button|small|" + state.items.GetName(id) + " : " + std::to_string(count) +
                "|left|" + idText + "|floating_" + idText + "|noflags|0|0|";
    }
    if (counts.empty())
        list = "\nadd_textbox|`4No floating items found in this world.|left|2480|";

    m_proxy.ShowDialog("set_default_color|`o"
                       "\nadd_label_with_icon|big|`wFloating Items``|left|6016|"
                       "\nadd_spacer|small|"
                       "\nadd_textbox|Press on the icon to be directed to the location of the item.|left|"
                       "\nadd_spacer|small|" +
                       list + DIALOG_END + "\nend_dialog|growscan_dialog|Cancel|Okay|");
}
