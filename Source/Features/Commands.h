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

private:
    bool Warp(string_view world);
    void Growscan();
    void ToggleAntigravity();

    void ShowWorldBlocks();
    void ShowDroppedItems();

    Proxy& m_proxy;
};
