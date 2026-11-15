#pragma once

#include "Core/Types.h"

class Proxy;
class TextPacket;

// Chat commands typed by the player (/warp, /growscan, /antigravity) and the dialogs they open.
// Handled input is not forwarded to the server.
class Commands {
public:
    explicit Commands(Proxy& proxy) : m_proxy(proxy) {}

    bool HandleChat(string_view text);
    bool HandleDialogReturn(const TextPacket& packet);

    // Actions behind the commands, also used by the UI.
    bool Warp(string_view world);
    void ToggleAntigravity();
    void PointToItem(int32 itemID); // shows the client's arrow pointing at a dropped item

private:
    void Growscan(); // opens the growscan dialog in the game
    void ShowWorldBlocks();
    void ShowDroppedItems();

    Proxy& m_proxy;
};
