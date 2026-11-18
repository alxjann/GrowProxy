#pragma once

#include "Core/Types.h"

class Proxy;

class Function {
public:
    explicit Function(Proxy& proxy) : m_proxy(proxy) {}

    bool Warp(string_view world);
    void ToggleAntigravity();

    void Growscan();
    void ShowWorldBlocks();
    void ShowDroppedItems();
    void PointToItem(int32 itemID);

private:
    Proxy& m_proxy;
};
