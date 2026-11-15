#pragma once

#include "Core/Types.h"
#include "UI/Theme.h"

#include <imgui.h>

// Custom-drawn controls that make up the proxy UI's look. Everything is immediate mode like ImGui itself.
namespace Widgets {
    enum class ButtonKind { Secondary, Primary, Danger, Ghost };

    // Sidebar navigation row: icon and label.
    bool NavItem(const char* icon, const char* label, bool selected);

    // A rounded panel with an optional icon + title row. size.y == 0 grows to fit the content.
    // Always pair with EndCard().
    void BeginCard(const char* id, ImVec2 size = ImVec2(0, 0), const char* icon = nullptr, const char* title = nullptr);
    void EndCard();

    // Icon + title row of a card. Leaves the cursor on the same line, so SameLine() can add controls to the right.
    void CardTitle(const char* icon, const char* title);

    // Moves the cursor so an item of the given width ends at the right edge of the content region.
    void AlignRight(float width);

    // Styled button; size 0 fits the label.
    bool Button(const char* label, ButtonKind kind = ButtonKind::Secondary, ImVec2 size = ImVec2(0, 0));

    // Square button with just an icon and a tooltip.
    bool IconButton(const char* id, const char* icon, const char* tooltip, ImVec4 color = Theme::TextMuted);

    // On/off switch with its label on the right, for toolbars (filters, layers). Returns true when it was flipped.
    bool Switch(const char* label, bool* value);
    float SwitchWidth(const char* label);

    // Toolbar flow: continues the current line when an item of the given width still fits, otherwise starts a
    // new line. This is what lets toolbars wrap in a narrow window.
    void SameLineIfFits(float width, float spacing = -1.0f);

    // Animated on/off switch. Returns true when it was flipped.
    bool Toggle(const char* id, bool* value);

    // Toggle with a title and a muted description on the left, the switch on the right.
    bool ToggleRow(const char* id, const char* title, const char* description, bool* value);

    // Small rounded label tinted with color.
    void Pill(const char* text, ImVec4 color);

    // Muted, uppercase, small section caption.
    void SectionLabel(const char* text);

    // "key ........ value" row, value right aligned.
    void KeyValue(const char* key, const char* value, ImVec4 valueColor = Theme::Text);

    // Text input with a search icon and hint. width <= 0 fills the remaining width.
    bool SearchBox(const char* id, char* buffer, size_t bufferSize, const char* hint, float width = 0.0f);

    // Horizontal group of mutually exclusive buttons. Returns true when the selection changed.
    bool Segmented(const char* id, int* current, const char* const labels[], int count);

    // Centered icon + title + hint for an empty list.
    void EmptyState(const char* icon, const char* title, const char* hint);

    // Thin horizontal bar filled to fraction (0..1).
    void MeterBar(float fraction, ImVec4 color, ImVec2 size);

    // Text in the given color at the given size.
    void TextColored(ImVec4 color, float size, const char* fmt, ...);

    // Hover tooltip for the last item.
    void Tooltip(const char* text);

    // Human readable byte count ("12.4 KB").
    string FormatBytes(uint64 bytes);

    // Thousands separators ("12,345").
    string FormatNumber(uint64 value);
}
