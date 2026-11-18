#include "UI/ProxyUI/ProxyUI.h"

#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/PacketCapture/PacketCapture.h"
#include "UI/Widgets/Widgets.h"
#include "Utils/Strings/Strings.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <variant>

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

    bool IsNoise(const CapturedPacket& packet) {
        switch (packet.gameType) {
            case PACKET_STATE:
            case PACKET_PING_REQUEST:
            case PACKET_PING_REPLY:
            case PACKET_SET_ICON_STATE:
                return true;
            default:
                return false;
        }
    }

    const char* TypeLabel(const CapturedPacket& packet) {
        return packet.gameType >= 0 ? PacketNames::Game(packet.gameType) : PacketNames::Message(packet.messageType);
    }

    ImVec4 DirectionColor(const CapturedPacket& packet) {
        if (packet.fromProxy)
            return Theme::FromProxy;
        return packet.to == Server ? Theme::ToServer : Theme::ToClient;
    }

    const char* DirectionLabel(const CapturedPacket& packet) {
        if (packet.fromProxy)
            return packet.to == Server ? ICON_FA_WAND_MAGIC_SPARKLES " Proxy \xE2\x86\x92 S" : ICON_FA_WAND_MAGIC_SPARKLES " Proxy \xE2\x86\x92 C";
        return packet.to == Server ? ICON_FA_ARROW_UP " To server" : ICON_FA_ARROW_DOWN " To client";
    }

    string Hex(const uint8* data, size_t size) {
        static const char* const DIGITS = "0123456789ABCDEF";
        string out;
        out.reserve(size * 3);
        for (size_t i = 0; i < size; ++i) {
            if (i)
                out += ' ';
            out += DIGITS[data[i] >> 4];
            out += DIGITS[data[i] & 15];
        }
        return out;
    }

    const char* VariantType(const Variant& value) {
        switch (value.index()) {
            case 1:
                return "float";
            case 2:
                return "string";
            case 3:
                return "vec2";
            case 4:
                return "vec3";
            case 5:
                return "uint";
            case 6:
                return "int";
            default:
                return "none";
        }
    }

    string VariantValue(const Variant& value) {
        char text[96];
        if (auto* f = std::get_if<float>(&value)) {
            snprintf(text, sizeof(text), "%g", *f);
            return text;
        }
        if (auto* s = std::get_if<string>(&value))
            return *s;
        if (auto* v = std::get_if<Vec2f>(&value)) {
            snprintf(text, sizeof(text), "%g, %g", v->x, v->y);
            return text;
        }
        if (auto* v = std::get_if<Vec3f>(&value)) {
            snprintf(text, sizeof(text), "%g, %g, %g", v->x, v->y, v->z);
            return text;
        }
        if (auto* u = std::get_if<uint32>(&value))
            return std::to_string(*u);
        if (auto* i = std::get_if<int32>(&value))
            return std::to_string(*i);
        return "";
    }

    void FieldRow(const char* name, const char* fmt, ...) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted);
        ImGui::TextUnformatted(name);
        ImGui::PopStyleColor();
        ImGui::TableNextColumn();

        char text[1024]; // fits every flag name
        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);
        Theme::PushMono();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
    }

    // key|value lines as a two column table; other lines span the row.
    void TextFields(string_view text) {
        if (!ImGui::BeginTable("##text_fields", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
            return;
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 120.0f * Scale());
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        size_t start = 0;
        while (start <= text.size()) {
            size_t end = text.find('\n', start);
            string_view line = text.substr(start, end == string_view::npos ? string_view::npos : end - start);
            start = end == string_view::npos ? text.size() + 1 : end + 1;
            if (line.empty())
                continue;

            size_t bar = line.find('|');
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted);
            ImGui::TextUnformatted(line.data(), line.data() + (bar == string_view::npos ? 0 : bar));
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            string_view value = bar == string_view::npos ? line : line.substr(bar + 1);
            Theme::PushMono();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(value.data(), value.data() + value.size());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
        }
        ImGui::EndTable();
    }

    // Game packet flag bits by index; nullptr = no known name. Names from GrowTrace's GamePacket.h.
    const char* const FLAG_NAMES[32] = {
        "BROADCAST",          nullptr,                   "NO_LERP",
        "EXTENDED_DATA",      "FACING_LEFT",             "ON_GROUND",
        "ON_BURN",            "ON_JUMP",                 "ON_KILL",
        "ON_PUNCH",           "ON_CREATE",               "SEND_ASAP",
        "ON_GOT_PUNCH",       "ON_RESPAWN",              "ON_GOT_OBJECT",
        "ON_TRAMPOLINE",      "ON_PRICK",                "ON_ICE",
        "HAND_ITEM_ACTIVE",   "BACK_ITEM_ACTIVE",        "UNDERWATER",
        "WALLHANG",           "CHARGING_PUNCH",          "CHARGING_PUNCH_END",
        "CHARGING_PUNCH_LEVEL_UP", "PLAY_HAY_CART_VISUALS", "DOUBLE_JUMP_ACTIVE",
        "EXTRA_TILE_INFO",    "ON_ACID",                 "TELEPORT",
        "ON_USE_ABILITY",     nullptr,
    };

    // "NAME | NAME | bit 1" for the set bits, "none" when no bit is set.
    string FlagList(uint32 flags) {
        string out;
        for (int bit = 0; bit < 32; ++bit) {
            if (!(flags & (1u << bit)))
                continue;
            if (!out.empty())
                out += " | ";
            out += FLAG_NAMES[bit] ? FLAG_NAMES[bit] : "bit " + std::to_string(bit);
        }
        return out.empty() ? "none" : out;
    }

    void HexDump(const uint8* data, size_t size) {
        constexpr size_t PER_LINE = 16;
        Theme::PushMono();
        ImGuiListClipper clipper;
        clipper.Begin((int)((size + PER_LINE - 1) / PER_LINE));
        while (clipper.Step()) {
            for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
                size_t offset = (size_t)line * PER_LINE;
                size_t count = std::min(PER_LINE, size - offset);

                char ascii[PER_LINE + 1] = {};
                for (size_t i = 0; i < count; ++i) {
                    uint8 c = data[offset + i];
                    ascii[i] = c >= 32 && c < 127 ? (char)c : '.';
                }
                string hex = Hex(data + offset, count);
                hex.resize(PER_LINE * 3 - 1, ' ');

                ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextFaint);
                ImGui::Text("%06zX", offset);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 14.0f * Scale());
                ImGui::TextUnformatted(hex.c_str());
                ImGui::SameLine(0, 14.0f * Scale());
                ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted);
                ImGui::TextUnformatted(ascii);
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopFont();
    }

}

bool ProxyUI::PacketVisible(const CapturedPacket& packet) const {
    if (m_packets.direction == 1 && packet.to != Server)
        return false;
    if (m_packets.direction == 2 && packet.to != Client)
        return false;
    if (m_packets.hideNoise && IsNoise(packet))
        return false;
    return true;
}

void ProxyUI::DrawPackets() {
    DrawPacketToolbar();
    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

    // filtered view of the capture buffer
    const auto& packets = m_capture.Packets();
    m_packets.visible.clear();
    for (int32 i = 0; i < (int32)packets.size(); ++i) {
        if (PacketVisible(packets[i]))
            m_packets.visible.push_back(i);
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float gap = ImGui::GetStyle().ItemSpacing.y;
    float table = std::floor((avail.y - gap) * 0.45f);
    DrawPacketTable(ImVec2(avail.x, table));
    DrawPacketDetails(ImVec2(avail.x, avail.y - table - gap));
}

void ProxyUI::DrawPacketToolbar() {
    if (m_capture.recording) {
        if (Widgets::Button(ICON_FA_CIRCLE_STOP "  Pause", Widgets::ButtonKind::Danger))
            m_capture.recording = false;
    } else if (Widgets::Button(ICON_FA_PLAY "  Record", Widgets::ButtonKind::Primary)) {
        m_capture.recording = true;
    }

    ImGui::SameLine(0, 6.0f * Scale());
    if (Widgets::Button(ICON_FA_ERASER "  Clear", Widgets::ButtonKind::Ghost)) {
        m_capture.Clear();
        m_packets.selected = 0;
    }

    ImGui::SameLine(0, 16.0f * Scale());
    static const char* const DIRECTIONS[] = {"All", ICON_FA_ARROW_UP "  To server", ICON_FA_ARROW_DOWN "  To client"};
    Widgets::Segmented("##direction", &m_packets.direction, DIRECTIONS, 3);

    // second row
    Widgets::Switch("Hide state & ping", &m_packets.hideNoise);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Auto-scroll"), 16.0f * Scale());
    Widgets::Switch("Auto-scroll", &m_packets.autoScroll);
}

void ProxyUI::DrawPacketTable(ImVec2 size) {
    const auto& packets = m_capture.Packets();
    Widgets::BeginCard("##packet_list", size);

    if (m_packets.visible.empty()) {
        if (packets.empty())
            Widgets::EmptyState(ICON_FA_ARROW_RIGHT_ARROW_LEFT, m_capture.recording ? "Waiting for traffic" : "Capture paused",
                                m_capture.recording ? "Packets between the game and the server appear here."
                                                    : "Press Record to capture packets.");
        else
            Widgets::EmptyState(ICON_FA_MAGNIFYING_GLASS, "No matching packets", "Change the filters to see more.");
        Widgets::EndCard();
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable;
    if (ImGui::BeginTable("##packets", 6, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.0f * Scale());
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 86.0f * Scale());
        ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed, 104.0f * Scale());
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Summary", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 56.0f * Scale());
        ImGui::TableHeadersRow();

        const float rowHeight = 26.0f * Scale();
        ImGuiListClipper clipper;
        clipper.Begin((int)m_packets.visible.size(), rowHeight);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const CapturedPacket& packet = packets[m_packets.visible[row]];
                ImGui::TableNextRow(0, rowHeight);
                ImGui::TableNextColumn();

                char id[32];
                snprintf(id, sizeof(id), "##pkt%llu", (unsigned long long)packet.number);
                bool selected = m_packets.selected == packet.number;
                if (ImGui::Selectable(id, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                                      ImVec2(0, rowHeight)))
                    m_packets.selected = packet.number;
                ImGui::SameLine(0, 0);

                float textOffset = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
                auto cell = [&](ImVec4 color, const char* text) {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text);
                    ImGui::PopStyleColor();
                };

                char text[32];
                snprintf(text, sizeof(text), "%llu", (unsigned long long)packet.number);
                cell(Theme::TextFaint, text);

                ImGui::TableNextColumn();
                cell(Theme::TextMuted, packet.clock);

                ImGui::TableNextColumn();
                cell(DirectionColor(packet), DirectionLabel(packet));

                ImGui::TableNextColumn();
                cell(Theme::TextMuted, TypeLabel(packet));

                ImGui::TableNextColumn();
                if (packet.blocked)
                    cell(Theme::Danger, (ICON_FA_BAN "  " + packet.summary).c_str());
                else
                    cell(Theme::Text, packet.summary.c_str());

                ImGui::TableNextColumn();
                cell(Theme::TextFaint, Widgets::FormatBytes(packet.data.size()).c_str());
            }
        }

        if (m_packets.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - rowHeight * 2.0f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndTable();
    }
    Widgets::EndCard();
}

void ProxyUI::DrawPacketDetails(ImVec2 size) {
    const auto& packets = m_capture.Packets();
    Widgets::BeginCard("##packet_details", size);

    // packet numbers are increasing, so the selection can be found with a binary search
    auto it = std::lower_bound(packets.begin(), packets.end(), m_packets.selected,
                               [](const CapturedPacket& p, uint64 number) { return p.number < number; });
    const CapturedPacket* packet = it != packets.end() && it->number == m_packets.selected ? &*it : nullptr;

    ImGui::BeginChild("##details_top");
    if (!packet) {
        Widgets::EmptyState(ICON_FA_CIRCLE_INFO, "No packet selected", "Select a packet to decode it.");
    } else {
        Widgets::Pill(DirectionLabel(*packet), DirectionColor(*packet));
        ImGui::SameLine(0, 6.0f * Scale());
        Widgets::Pill(TypeLabel(*packet), Theme::TextMuted);
        if (packet->blocked) {
            ImGui::SameLine(0, 6.0f * Scale());
            Widgets::Pill(ICON_FA_BAN "  blocked", Theme::Danger);
        }

        Widgets::TextColored(Theme::Text, Theme::FONT_LARGE, "%s", packet->summary.c_str());
        Widgets::TextColored(Theme::TextFaint, Theme::FONT_SMALL, "#%llu  \xC2\xB7  %s  \xC2\xB7  %zu bytes",
                             (unsigned long long)packet->number, packet->clock, packet->data.size());
        ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

        if (Widgets::Button(ICON_FA_COPY "  Copy hex", Widgets::ButtonKind::Ghost))
            ImGui::SetClipboardText(Hex(packet->data.data(), packet->data.size()).c_str());

        ImGui::SameLine();
        static const char* const TABS[] = {"Decoded", "Hex"};
        Widgets::AlignRight(ImGui::CalcTextSize("DecodedHex").x + ImGui::GetStyle().FramePadding.x * 4.0f + 6.0f * Scale());
        Widgets::Segmented("##detail_tab", &m_packets.detailTab, TABS, 2);
        ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

        ImGui::BeginChild("##details_body", ImVec2(0, 0));
        PacketView view = packet->View();
        if (m_packets.detailTab == 1) {
            HexDump(packet->data.data(), packet->data.size());
        } else if (packet->messageType != MESSAGE_GAME_PACKET) {
            Widgets::SectionLabel(PacketNames::Message(packet->messageType));
            TextFields(view.Text());
        } else {
            GameUpdatePacket game;
            const uint8* extra = nullptr;
            if (!view.GetGamePacket(game, &extra)) {
                Widgets::TextColored(Theme::Warning, Theme::FONT_BODY, "Truncated game packet");
                HexDump(packet->data.data(), packet->data.size());
            } else {
                VariantList call;
                if (game.type == PACKET_CALL_FUNCTION && extra && call.Parse(extra, game.extraDataSize)) {
                    Widgets::SectionLabel("Function call");
                    if (ImGui::BeginTable("##variants", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24.0f * Scale());
                        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 56.0f * Scale());
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        for (size_t i = 0; i < call.Size(); ++i) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            Widgets::TextColored(Theme::TextFaint, Theme::FONT_BODY, "%zu", i);
                            ImGui::TableNextColumn();
                            Widgets::TextColored(Theme::Violet, Theme::FONT_SMALL, "%s", VariantType(call[i]));
                            ImGui::TableNextColumn();
                            string value = VariantValue(call[i]);
                            Theme::PushMono();
                            ImGui::PushTextWrapPos(0.0f);
                            ImGui::TextUnformatted(value.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::PopFont();
                        }
                        ImGui::EndTable();
                    }
                    ImGui::Dummy(ImVec2(0, 6.0f * Scale()));
                }

                Widgets::SectionLabel("Game packet fields");
                if (ImGui::BeginTable("##fields", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120.0f * Scale());
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    FieldRow("type", "%u  (0x%02X, %s)", game.type, game.type, PacketNames::Game(game.type));
                    FieldRow("byte1 / 2 / 3", "%u / %u / %u", game.byte1, game.byte2, game.byte3);
                    FieldRow("netID", "%d", game.netID);
                    FieldRow("targetID", "%d", game.targetID);
                    FieldRow("flags", "0x%08X", game.flags);
                    FieldRow("active flags", "%s", FlagList(game.flags).c_str());
                    FieldRow("floatValue", "%g", game.floatValue);
                    FieldRow("value", "%d", game.value);
                    FieldRow("pos", "%.1f, %.1f", game.pos.x, game.pos.y);
                    FieldRow("speed", "%.1f, %.1f", game.speed.x, game.speed.y);
                    FieldRow("rotation", "%g", game.rotation);
                    FieldRow("tile", "%d, %d", game.tileX, game.tileY);
                    FieldRow("extraDataSize", "%u", game.extraDataSize);
                    ImGui::EndTable();
                }

                if (extra) {
                    ImGui::Dummy(ImVec2(0, 6.0f * Scale()));
                    Widgets::SectionLabel("Extended data");
                    HexDump(extra, game.extraDataSize);
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    Widgets::EndCard();
}
