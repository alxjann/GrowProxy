#include "UI/ProxyUI.h"

#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/Widgets.h"
#include "Utils/Strings.h"

#include <algorithm>

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

    bool Matches(const string& name, int32 id, const string& filterLower) {
        if (filterLower.empty())
            return true;
        if (std::to_string(id) == filterLower)
            return true;
        return Strings::ToLower(name).find(filterLower) != string::npos;
    }

    const char* RarityLabel(int16 rarity) {
        if (rarity >= 999 || rarity <= 0)
            return "-";
        static char text[16];
        snprintf(text, sizeof(text), "%d", rarity);
        return text;
    }

    string FormatDuration(uint32 seconds) {
        if (seconds == 0)
            return "-";
        char text[48];
        uint32 days = seconds / 86400, hours = seconds / 3600 % 24, minutes = seconds / 60 % 60, secs = seconds % 60;
        if (days)
            snprintf(text, sizeof(text), "%ud %uh %um", days, hours, minutes);
        else if (hours)
            snprintf(text, sizeof(text), "%uh %um %us", hours, minutes, secs);
        else if (minutes)
            snprintf(text, sizeof(text), "%um %us", minutes, secs);
        else
            snprintf(text, sizeof(text), "%us", secs);
        return text;
    }

}

void ProxyUI::DrawInventory() {
    const GameState& state = m_proxy.State();
    const Inventory& inventory = state.inventory;
    const auto& items = inventory.GetItems();

    // toolbar: search on the left, slot usage on the right
    Widgets::SearchBox("##inv_search", m_inventorySearch, sizeof(m_inventorySearch), "Search by name or ID",
                       320.0f * Scale());
    if (inventory.GetSlots() > 0) {
        ImGui::SameLine();
        char text[48];
        snprintf(text, sizeof(text), "%zu / %u slots", items.size(), inventory.GetSlots());
        float meter = 140.0f * Scale();
        Widgets::AlignRight(ImGui::CalcTextSize(text).x + meter + ImGui::GetStyle().ItemSpacing.x);
        ImGui::AlignTextToFramePadding();
        Widgets::TextColored(Theme::TextMuted, Theme::FONT_BODY, "%s", text);
        ImGui::SameLine();
        float fraction = (float)items.size() / (float)inventory.GetSlots();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - 6.0f * Scale()) * 0.5f);
        Widgets::MeterBar(fraction, fraction > 0.9f ? Theme::Warning : Theme::Accent, ImVec2(meter, 6.0f * Scale()));
    }
    ImGui::Dummy(ImVec2(0, 4.0f * Scale()));

    if (items.empty()) {
        Widgets::EmptyState(ICON_FA_BOX_OPEN, "Inventory is empty",
                            "Your backpack appears once the server sends it after login.");
        return;
    }

    struct Row {
        const InventoryItem* entry;
        string name;
        const Item* item;
    };
    std::vector<Row> rows;
    string filter = Strings::ToLower(m_inventorySearch);
    for (const InventoryItem& entry : items) {
        string name = state.items.GetName(entry.itemID);
        if (Matches(name, entry.itemID, filter))
            rows.push_back({&entry, std::move(name), state.items.Get(entry.itemID)});
    }

    Widgets::BeginCard("##inventory", ImVec2(0, ImGui::GetContentRegionAvail().y));
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##inv_table", 4, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_DefaultSort, 3.0f);
        ImGui::TableSetupColumn("ID", 0, 0.8f);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_PreferSortDescending, 2.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_NoSort, 1.2f);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
            const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
            bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
            auto less = [&](const Row& a, const Row& b) {
                switch (spec.ColumnIndex) {
                    case 1:
                        return a.entry->itemID < b.entry->itemID;
                    case 2:
                        return a.entry->count < b.entry->count;
                    default:
                        return Strings::ToLower(a.name) < Strings::ToLower(b.name);
                }
            };
            std::stable_sort(rows.begin(), rows.end(),
                             [&](const Row& a, const Row& b) { return ascending ? less(a, b) : less(b, a); });
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)rows.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const Row& row = rows[i];
                ImGui::TableNextRow(0, 34.0f * Scale());

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(row.name.c_str());

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                Widgets::TextColored(Theme::TextMuted, Theme::FONT_BODY, "%u", row.entry->itemID);

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%u", row.entry->count);

                ImGui::TableNextColumn();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f * Scale());
                if (row.entry->IsEquipped())
                    Widgets::Pill(ICON_FA_SHIRT "  Equipped", Theme::Accent);
            }
        }
        ImGui::EndTable();
    }
    Widgets::EndCard();
}

void ProxyUI::DrawItems() {
    const ItemDatabase& database = m_proxy.State().items;
    if (database.Count() == 0) {
        Widgets::EmptyState(ICON_FA_CUBES, "No item database yet",
                            "The proxy reads items.dat when the game downloads it at login.");
        return;
    }

    // rebuild the result list only when the query or the database changes
    string query = Strings::ToLower(Strings::Trim(m_items.search));
    if (query != m_items.lastQuery || database.Count() != m_items.lastCount) {
        m_items.lastQuery = query;
        m_items.lastCount = database.Count();
        m_items.results.clear();
        for (int32 id = 0; id < (int32)database.Count(); ++id) {
            const Item* item = database.Get(id);
            if (item && Matches(item->name, item->id, query))
                m_items.results.push_back(item->id);
        }
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float gap = ImGui::GetStyle().ItemSpacing.x * 1.6f;
    float listWidth = std::min(380.0f * Scale(), avail.x * 0.45f);

    // list
    Widgets::BeginCard("##item_list", ImVec2(listWidth, avail.y));
    Widgets::SearchBox("##item_search", m_items.search, sizeof(m_items.search), "Search items by name or ID");
    Widgets::TextColored(Theme::TextFaint, Theme::FONT_SMALL, "%s results",
                         Widgets::FormatNumber(m_items.results.size()).c_str());

    ImGui::BeginChild("##item_scroll", ImVec2(0, 0));
    ImGuiListClipper clipper;
    clipper.Begin((int)m_items.results.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const Item* item = database.Get(m_items.results[i]);
            if (!item)
                continue;
            char label[160];
            snprintf(label, sizeof(label), "%s##item%d", item->name.c_str(), item->id);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            if (ImGui::Selectable(label, m_items.selected == item->id, 0, ImVec2(0, 28.0f * Scale())))
                m_items.selected = item->id;
            ImGui::PopStyleVar();

            ImGui::SameLine();
            char id[16];
            snprintf(id, sizeof(id), "#%d", item->id);
            Widgets::AlignRight(ImGui::CalcTextSize(id).x + 6.0f * Scale());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (28.0f * Scale() - ImGui::GetTextLineHeight()) * 0.5f);
            Widgets::TextColored(Theme::TextFaint, Theme::FONT_BODY, "%s", id);
        }
    }
    ImGui::EndChild();
    Widgets::EndCard();

    ImGui::SameLine(0, gap);

    // details
    Widgets::BeginCard("##item_details", ImVec2(0, avail.y));
    const Item* item = database.Get(m_items.selected);
    if (!item) {
        Widgets::EmptyState(ICON_FA_CUBE, "Select an item", "Pick an item on the left to see its definition.");
        Widgets::EndCard();
        return;
    }

    Widgets::TextColored(Theme::TextFaint, Theme::FONT_SMALL, "ITEM #%d", item->id);
    Widgets::TextColored(Theme::Text, Theme::FONT_HERO, "%s", item->name.c_str());
    ImGui::Dummy(ImVec2(0, 4.0f * Scale()));

    if (!item->description.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextMuted);
        ImGui::TextWrapped("%s", item->description.c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6.0f * Scale()));
    }

    char text[96];
    Widgets::SectionLabel("Properties");
    snprintf(text, sizeof(text), "%u", item->type);
    Widgets::KeyValue("Type", text);
    static const char* const COLLISIONS[] = {"None", "Solid", "Platform", "Gateway", "Solid when off",
                                             "One way", "VIP", "Solid from below"};
    Widgets::KeyValue("Collision", item->collisionType < 8 ? COLLISIONS[item->collisionType] : "Unknown");
    snprintf(text, sizeof(text), "%u", item->material);
    Widgets::KeyValue("Material", text);
    Widgets::KeyValue("Rarity", RarityLabel(item->rarity));
    snprintf(text, sizeof(text), "%u", item->maxAmount);
    Widgets::KeyValue("Max amount", text);

    ImGui::Dummy(ImVec2(0, 6.0f * Scale()));
    Widgets::SectionLabel("Farming & texture");
    snprintf(text, sizeof(text), "%u", item->breakHits / 6); // items.dat stores hits x 6
    Widgets::KeyValue("Break hits", text);
    Widgets::KeyValue("Grow time", FormatDuration(item->growTime).c_str());
    Widgets::KeyValue("Texture", item->texture.empty() ? "-" : item->texture.c_str());
    snprintf(text, sizeof(text), "%u, %u", item->textureX, item->textureY);
    Widgets::KeyValue("Texture cell", text);
    snprintf(text, sizeof(text), "0x%04X", item->flags);
    Widgets::KeyValue("Flags", text);

    Widgets::EndCard();
}
