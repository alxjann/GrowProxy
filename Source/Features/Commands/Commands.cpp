#include "Features/Commands/Commands.h"

#include "Packets/TextPacket/TextPacket.h"
#include "Proxy/Proxy.h"
#include "Utils/Strings/Strings.h"

bool Commands::HandleChat(string_view text) {
    text = Strings::Trim(text);
    if (text.empty() || text[0] != '/')
        return false;

    // "/name arguments"
    size_t space = text.find(' ');
    string name = Strings::ToLower(text.substr(1, space == string_view::npos ? string_view::npos : space - 1));
    string_view args = space == string_view::npos ? string_view() : Strings::Trim(text.substr(space + 1));

    if (name == "warp")
        return m_proxy.GetFunction().Warp(args);

    if (name == "growscan" || name == "gscan" || name == "gs") {
        m_proxy.ConsoleMessage("`6" + string(text));
        m_proxy.GetFunction().Growscan();
        return true;
    }

    if (name == "antigravity" || name == "ag") {
        m_proxy.GetFunction().ToggleAntigravity();
        return true;
    }

    return false;
}

bool Commands::HandleDialogReturn(const TextPacket& packet) {
    string_view button = packet.Get("buttonClicked");

    if (button == "gscan_block") {
        m_proxy.GetFunction().ShowWorldBlocks();
        return true;
    }
    if (button == "gscan_float") {
        m_proxy.GetFunction().ShowDroppedItems();
        return true;
    }

    // item buttons in the floating items list: point the arrow at the item
    for (string_view prefix : {string_view("floating_"), string_view("findObject_")}) {
        if (Strings::StartsWith(button, prefix)) {
            m_proxy.GetFunction().PointToItem(Strings::ToInt(button.substr(prefix.size())).value_or(0));
            return true;
        }
    }

    return Strings::StartsWith(packet.Get("dialog_name"), "growscan");
}
