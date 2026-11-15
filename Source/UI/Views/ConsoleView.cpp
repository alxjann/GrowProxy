#include "UI/ProxyUI.h"

#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/Widgets.h"
#include "Utils/Strings.h"

#include <cstring>

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

    ImVec4 LevelColor(Log::Level level) {
        switch (level) {
            case Log::Level::Warn:
                return Theme::Warning;
            case Log::Level::Error:
                return Theme::Danger;
            default:
                return Theme::Info;
        }
    }

    const char* LevelName(Log::Level level) {
        switch (level) {
            case Log::Level::Warn:
                return "WARN";
            case Log::Level::Error:
                return "ERROR";
            default:
                return "INFO";
        }
    }

}

void ProxyUI::DrawConsole() {
    ConsoleView& console = m_console;

    // toolbar
    const float spacing = 16.0f * Scale();
    Widgets::Switch("Info", &console.showInfo);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Warnings"), spacing);
    Widgets::Switch("Warnings", &console.showWarn);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Errors"), spacing);
    Widgets::Switch("Errors", &console.showError);
    Widgets::SameLineIfFits(Widgets::SwitchWidth("Auto-scroll"), spacing);
    Widgets::Switch("Auto-scroll", &console.autoScroll);
    Widgets::SameLineIfFits(90.0f * Scale(), spacing);
    if (Widgets::Button(ICON_FA_ERASER "  Clear", Widgets::ButtonKind::Ghost))
        console.entries.clear();

    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

    // log list
    ImVec2 avail = ImGui::GetContentRegionAvail();

    std::vector<int32> visible;
    for (int32 i = 0; i < (int32)console.entries.size(); ++i) {
        const Log::Entry& entry = console.entries[i];
        bool shown = entry.level == Log::Level::Info ? console.showInfo
                     : entry.level == Log::Level::Warn ? console.showWarn
                                                       : console.showError;
        if (shown)
            visible.push_back(i);
    }

    Widgets::BeginCard("##log", avail);
    if (visible.empty()) {
        Widgets::EmptyState(ICON_FA_TERMINAL, console.entries.empty() ? "Log is empty" : "No matching messages",
                            console.entries.empty() ? "Proxy messages appear here." : "Change the filters to see more.");
    } else {
        ImGui::BeginChild("##log_scroll", ImVec2(0, 0), 0, ImGuiWindowFlags_HorizontalScrollbar);
        const float levelWidth = 54.0f * Scale();
        ImGuiListClipper clipper;
        clipper.Begin((int)visible.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const Log::Entry& entry = console.entries[visible[row]];
                Theme::PushMono();
                ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextFaint);
                ImGui::Text("%02d:%02d:%02d", entry.hour, entry.minute, entry.second);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 12.0f * Scale());

                float x = ImGui::GetCursorPosX();
                ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.level));
                ImGui::TextUnformatted(LevelName(entry.level));
                ImGui::PopStyleColor();
                ImGui::SameLine(x + levelWidth);

                ImGui::PushStyleColor(ImGuiCol_Text, entry.level == Log::Level::Info ? Theme::Text : LevelColor(entry.level));
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
        }
        if (console.scrollToBottom || (console.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f))
            ImGui::SetScrollHereY(1.0f);
        console.scrollToBottom = false;
        ImGui::EndChild();
    }
    Widgets::EndCard();
}
