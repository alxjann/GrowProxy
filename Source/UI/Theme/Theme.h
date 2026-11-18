#pragma once

#include "Core/Types.h"

#include <imgui.h>

// Colors, sizes and fonts of the proxy UI. One dark palette with a blue accent.
namespace Theme {
    constexpr ImVec4 Hex(uint32 rgb, float alpha = 1.0f) {
        return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
    }

    inline ImVec4 WithAlpha(ImVec4 color, float alpha) {
        color.w = alpha;
        return color;
    }

    inline ImU32 U32(ImVec4 color) { return ImGui::ColorConvertFloat4ToU32(color); }

    // Surfaces, darkest first.
    constexpr ImVec4 Background = Hex(0x0B0D10);
    constexpr ImVec4 Sidebar = Hex(0x0F1216);
    constexpr ImVec4 Surface = Hex(0x14181D);  // cards
    constexpr ImVec4 Surface2 = Hex(0x1A1F26); // inputs, buttons
    constexpr ImVec4 Surface3 = Hex(0x232933); // hovered
    constexpr ImVec4 Border = Hex(0x232A33);
    constexpr ImVec4 BorderStrong = Hex(0x2E3642);

    constexpr ImVec4 Text = Hex(0xE6EAF0);
    constexpr ImVec4 TextMuted = Hex(0x8B95A5);
    constexpr ImVec4 TextFaint = Hex(0x5B6575);

    constexpr ImVec4 Accent = Hex(0x3B82F6);
    constexpr ImVec4 AccentHover = Hex(0x60A5FA);
    constexpr ImVec4 AccentStrong = Hex(0x2563EB);
    constexpr ImVec4 Info = Hex(0x2DD4BF); // teal, so it stays apart from the blue accent
    constexpr ImVec4 Violet = Hex(0xA78BFA);
    constexpr ImVec4 Warning = Hex(0xFBBF24);
    constexpr ImVec4 Danger = Hex(0xF87171);

    // Packet directions.
    constexpr ImVec4 ToServer = Info;
    constexpr ImVec4 ToClient = Violet;
    constexpr ImVec4 FromProxy = Accent;

    // Font sizes (unscaled; the DPI scale is applied globally).
    constexpr float FONT_SMALL = 13.0f;
    constexpr float FONT_BODY = 15.0f;
    constexpr float FONT_LARGE = 18.0f;
    constexpr float FONT_TITLE = 22.0f;
    constexpr float FONT_HERO = 26.0f;

    constexpr float SIDEBAR_WIDTH = 216.0f;
    constexpr float HEADER_HEIGHT = 64.0f;
    constexpr float CARD_ROUNDING = 10.0f;

    // Applies the style and loads the fonts. dpiScale is the monitor content scale (1 at 96 DPI).
    void Apply(float dpiScale);

    // Pushes the UI font at a different size; pair with ImGui::PopFont().
    void PushFontSize(float size);

    // Pushes the monospace font (hex dumps, raw packet text); pair with ImGui::PopFont().
    void PushMono(float size = FONT_BODY - 1.0f);
}
