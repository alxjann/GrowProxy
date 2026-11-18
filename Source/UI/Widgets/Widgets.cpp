#include "UI/Widgets/Widgets.h"

#include "UI/Icons.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {

    // Moves an animated value toward target, framerate independent.
    float Approach(ImGuiID id, float target, float speed = 14.0f) {
        ImGuiStorage* storage = ImGui::GetStateStorage();
        float value = storage->GetFloat(id, target);
        float t = std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
        value += (target - value) * t;
        if (std::fabs(target - value) < 0.001f)
            value = target;
        storage->SetFloat(id, value);
        return value;
    }

    ImVec4 Lerp(ImVec4 a, ImVec4 b, float t) {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

}

bool Widgets::NavItem(const char* icon, const char* label, bool selected) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw = window->DrawList;

    const float height = 38.0f * Scale();
    const float width = ImGui::GetContentRegionAvail().x;
    ImVec2 min = ImGui::GetCursorScreenPos();
    ImVec2 max(min.x + width, min.y + height);

    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##nav", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    float hover = Approach(ImGui::GetID(label), hovered || selected ? 1.0f : 0.0f);
    if (selected)
        draw->AddRectFilled(min, max, Theme::U32(Theme::Surface2), 8.0f * Scale());
    else if (hover > 0.0f)
        draw->AddRectFilled(min, max, Theme::U32(Theme::Hex(0xFFFFFF, 0.035f * hover)), 8.0f * Scale());

    if (selected) {
        float barHeight = height * 0.45f;
        ImVec2 barMin(min.x, min.y + (height - barHeight) * 0.5f);
        draw->AddRectFilled(barMin, ImVec2(barMin.x + 3.0f * Scale(), barMin.y + barHeight), Theme::U32(Theme::Accent),
                            2.0f * Scale());
    }

    const float padding = 14.0f * Scale();
    const float fontSize = ImGui::GetFontSize();
    float textY = min.y + (height - fontSize) * 0.5f;
    ImVec4 iconColor = selected ? Theme::Accent : Lerp(Theme::TextFaint, Theme::TextMuted, hover);
    ImVec4 textColor = selected ? Theme::Text : Lerp(Theme::TextMuted, Theme::Text, hover);
    draw->AddText(ImVec2(min.x + padding, textY), Theme::U32(iconColor), icon);
    draw->AddText(ImVec2(min.x + padding + fontSize + 12.0f * Scale(), textY), Theme::U32(textColor), label);
    return clicked;
}

void Widgets::BeginCard(const char* id, ImVec2 size, const char* icon, const char* title) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Surface);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::Border);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * Scale(), 14.0f * Scale()));

    ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
    if (size.y == 0.0f)
        flags |= ImGuiChildFlags_AutoResizeY;
    ImGui::BeginChild(id, size, flags, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    if (title) {
        CardTitle(icon, title);
        ImGui::Dummy(ImVec2(0.0f, 2.0f * Scale()));
    }
}

void Widgets::CardTitle(const char* icon, const char* title) {
    if (icon) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Accent);
        ImGui::TextUnformatted(icon);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 8.0f * Scale());
    }
    ImGui::TextUnformatted(title);
}

void Widgets::AlignRight(float width) {
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
}

void Widgets::EndCard() {
    ImGui::EndChild();
}

bool Widgets::Button(const char* label, ButtonKind kind, ImVec2 size) {
    int colors = 0;
    auto push = [&](ImGuiCol index, ImVec4 color) {
        ImGui::PushStyleColor(index, color);
        colors++;
    };

    switch (kind) {
        case ButtonKind::Primary:
            push(ImGuiCol_Button, Theme::Accent);
            push(ImGuiCol_ButtonHovered, Theme::AccentHover);
            push(ImGuiCol_ButtonActive, Theme::AccentStrong);
            push(ImGuiCol_Text, Theme::Hex(0xFFFFFF));
            break;
        case ButtonKind::Danger:
            push(ImGuiCol_Button, Theme::WithAlpha(Theme::Danger, 0.12f));
            push(ImGuiCol_ButtonHovered, Theme::WithAlpha(Theme::Danger, 0.22f));
            push(ImGuiCol_ButtonActive, Theme::WithAlpha(Theme::Danger, 0.30f));
            push(ImGuiCol_Text, Theme::Danger);
            break;
        case ButtonKind::Ghost:
            push(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            push(ImGuiCol_ButtonHovered, Theme::Surface2);
            push(ImGuiCol_ButtonActive, Theme::Surface3);
            push(ImGuiCol_Text, Theme::TextMuted);
            break;
        case ButtonKind::Secondary:
            break;
    }

    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(colors);
    return clicked;
}

bool Widgets::IconButton(const char* id, const char* icon, const char* tooltip, ImVec4 color) {
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::Surface3);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::BorderStrong);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * Scale(), 4.0f * Scale()));
    bool clicked = ImGui::Button(icon);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    if (tooltip)
        Tooltip(tooltip);
    ImGui::PopID();
    return clicked;
}

namespace {

    constexpr float SWITCH_GAP = 8.0f; // between a switch and its label

    ImVec2 SwitchSize() {
        float height = ImGui::GetFontSize() * 1.25f;
        return ImVec2(height * 1.8f, height);
    }

    void DrawSwitch(ImVec2 pos, ImGuiID id, bool value, bool hovered) {
        const ImVec2 size = SwitchSize();
        float t = Approach(id, value ? 1.0f : 0.0f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec4 track = Lerp(hovered ? Theme::BorderStrong : Theme::Surface3, Theme::Accent, t);
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Theme::U32(track), size.y * 0.5f);

        float radius = size.y * 0.5f - 2.5f * Scale();
        float x = pos.x + size.y * 0.5f + (size.x - size.y) * t;
        draw->AddCircleFilled(ImVec2(x, pos.y + size.y * 0.5f), radius, Theme::U32(Theme::Hex(0xF8FAFC)), 24);
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

}

float Widgets::SwitchWidth(const char* label) {
    return SwitchSize().x + SWITCH_GAP * Scale() + ImGui::CalcTextSize(label).x;
}

bool Widgets::Switch(const char* label, bool* value) {
    const ImVec2 size = SwitchSize();
    const float frame = ImGui::GetFrameHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::InvisibleButton(label, ImVec2(SwitchWidth(label), frame));
    if (clicked)
        *value = !*value;
    bool hovered = ImGui::IsItemHovered();

    DrawSwitch(ImVec2(pos.x, pos.y + (frame - size.y) * 0.5f), ImGui::GetID(label), *value, hovered);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(pos.x + size.x + SWITCH_GAP * Scale(), pos.y + (frame - ImGui::GetFontSize()) * 0.5f),
        Theme::U32(*value ? Theme::Text : Theme::TextMuted), label);
    return clicked;
}

void Widgets::SameLineIfFits(float width, float spacing) {
    ImGui::SameLine(0, spacing);
    if (ImGui::GetContentRegionAvail().x < width)
        ImGui::NewLine();
}

bool Widgets::Toggle(const char* id, bool* value) {
    const float height = ImGui::GetFontSize() * 1.25f;
    const float width = height * 1.8f;
    ImVec2 pos = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    if (clicked)
        *value = !*value;
    bool hovered = ImGui::IsItemHovered();

    DrawSwitch(pos, ImGui::GetID(id), *value, hovered);
    return clicked;
}

bool Widgets::ToggleRow(const char* id, const char* title, const char* description, bool* value) {
    const float switchWidth = ImGui::GetFontSize() * 1.25f * 1.8f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
    ImVec2 start = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - switchWidth - spacing);
    ImGui::TextUnformatted(title);
    if (description) {
        Theme::PushFontSize(Theme::FONT_SMALL);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted);
        ImGui::TextWrapped("%s", description);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImVec2 groupMin = ImGui::GetItemRectMin();
    ImVec2 groupMax = ImGui::GetItemRectMax();
    float switchHeight = ImGui::GetFontSize() * 1.25f;

    ImGui::SetCursorScreenPos(ImVec2(start.x + width - switchWidth, groupMin.y + (groupMax.y - groupMin.y - switchHeight) * 0.5f));
    bool changed = Toggle(id, value);

    ImGui::SetCursorScreenPos(ImVec2(start.x, std::max(groupMax.y, ImGui::GetItemRectMax().y)));
    ImGui::Dummy(ImVec2(0, 0));
    return changed;
}

void Widgets::Pill(const char* text, ImVec4 color) {
    Theme::PushFontSize(Theme::FONT_SMALL);
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 padding(8.0f * Scale(), 2.0f * Scale());
    ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Theme::U32(Theme::WithAlpha(color, 0.14f)),
                        size.y * 0.5f);
    draw->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y), Theme::U32(color), text);
    ImGui::PopFont();
}

void Widgets::SectionLabel(const char* text) {
    Theme::PushFontSize(Theme::FONT_SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextFaint);
    char upper[128];
    size_t i = 0;
    for (; text[i] && i < sizeof(upper) - 1; ++i)
        upper[i] = (char)toupper((unsigned char)text[i]);
    upper[i] = '\0';
    ImGui::TextUnformatted(upper);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void Widgets::KeyValue(const char* key, const char* value, ImVec4 valueColor) {
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted);
    ImGui::TextUnformatted(key);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    float width = ImGui::CalcTextSize(value).x;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);

    ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

bool Widgets::SearchBox(const char* id, char* buffer, size_t bufferSize, const char* hint, float width) {
    ImGuiStyle& style = ImGui::GetStyle();
    float iconWidth = ImGui::GetFontSize();

    ImGui::SetNextItemWidth(width > 0.0f ? width : ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x + iconWidth + 6.0f * Scale(), style.FramePadding.y));
    bool changed = ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
    ImGui::PopStyleVar();

    ImVec2 min = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + style.FramePadding.x, min.y + style.FramePadding.y),
                                        Theme::U32(Theme::TextFaint), ICON_FA_MAGNIFYING_GLASS);

    if (buffer[0] != '\0') {
        // clear button inside the right edge
        ImVec2 max = ImGui::GetItemRectMax();
        float size = ImGui::GetFrameHeight();
        ImVec2 backup = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(max.x - size, min.y));
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextFaint);
        if (ImGui::Button(ICON_FA_XMARK, ImVec2(size, size))) {
            buffer[0] = '\0';
            changed = true;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        ImGui::SetCursorScreenPos(backup);
        ImGui::Dummy(ImVec2(0, 0));
    }
    return changed;
}

bool Widgets::Segmented(const char* id, int* current, const char* const labels[], int count) {
    ImGui::PushID(id);
    ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const float height = ImGui::GetFrameHeight();
    const float inset = 3.0f * Scale();
    float total = inset * 2.0f;
    for (int i = 0; i < count; ++i)
        total += ImGui::CalcTextSize(labels[i], nullptr, true).x + style.FramePadding.x * 2.0f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    draw->AddRectFilled(pos, ImVec2(pos.x + total, pos.y + height), Theme::U32(Theme::Surface2), style.FrameRounding);

    bool changed = false;
    float x = pos.x + inset;
    for (int i = 0; i < count; ++i) {
        float width = ImGui::CalcTextSize(labels[i], nullptr, true).x + style.FramePadding.x * 2.0f;
        ImVec2 min(x, pos.y + inset);
        ImVec2 max(x + width, pos.y + height - inset);

        ImGui::SetCursorScreenPos(min);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##seg", ImVec2(width, max.y - min.y)) && *current != i) {
            *current = i;
            changed = true;
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        bool selected = *current == i;
        if (selected)
            draw->AddRectFilled(min, max, Theme::U32(Theme::Surface3), style.FrameRounding - inset * 0.5f);

        ImVec4 color = selected ? Theme::Text : (hovered ? Theme::Text : Theme::TextMuted);
        ImVec2 textSize = ImGui::CalcTextSize(labels[i], nullptr, true);
        draw->AddText(ImVec2(min.x + (width - textSize.x) * 0.5f, min.y + (max.y - min.y - textSize.y) * 0.5f),
                      Theme::U32(color), labels[i], ImGui::FindRenderedTextEnd(labels[i]));
        x += width;
    }

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(total, height));
    ImGui::PopID();
    return changed;
}

void Widgets::EmptyState(const char* icon, const char* title, const char* hint) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float blockHeight = ImGui::GetFontSize() * 5.0f;
    ImVec2 start = ImGui::GetCursorPos();
    float y = start.y + std::max(0.0f, (avail.y - blockHeight) * 0.5f);

    auto centered = [&](const char* text, ImVec4 color, float size) {
        Theme::PushFontSize(size);
        float width = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPos(ImVec2(start.x + std::max(0.0f, (avail.x - width) * 0.5f), y));
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        y += ImGui::GetTextLineHeightWithSpacing() + 2.0f * Scale();
        ImGui::PopFont();
    };

    centered(icon, Theme::TextFaint, 30.0f);
    y += 4.0f * Scale();
    centered(title, Theme::Text, Theme::FONT_LARGE);
    if (hint)
        centered(hint, Theme::TextMuted, Theme::FONT_SMALL);
}

void Widgets::MeterBar(float fraction, ImVec4 color, ImVec2 size) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (size.x <= 0.0f)
        size.x = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(size);

    fraction = std::clamp(fraction, 0.0f, 1.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float rounding = size.y * 0.5f;
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Theme::U32(Theme::Surface3), rounding);
    if (fraction > 0.0f)
        draw->AddRectFilled(pos, ImVec2(pos.x + std::max(size.y, size.x * fraction), pos.y + size.y),
                            Theme::U32(color), rounding);
}

void Widgets::TextColored(ImVec4 color, float size, const char* fmt, ...) {
    Theme::PushFontSize(size);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void Widgets::Tooltip(const char* text) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * Scale(), 6.0f * Scale()));
    ImGui::SetTooltip("%s", text);
    ImGui::PopStyleVar();
}

string Widgets::FormatBytes(uint64 bytes) {
    static const char* const UNITS[] = {"B", "KB", "MB", "GB"};
    double value = (double)bytes;
    int unit = 0;
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        unit++;
    }

    char text[32];
    snprintf(text, sizeof(text), unit == 0 ? "%.0f %s" : "%.1f %s", value, UNITS[unit]);
    return text;
}

string Widgets::FormatNumber(uint64 value) {
    string digits = std::to_string(value);
    string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count && count % 3 == 0)
            out.insert(out.begin(), ',');
        out.insert(out.begin(), *it);
        count++;
    }
    return out;
}
