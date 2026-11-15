#include "UI/Theme.h"

#include "UI/Fonts.h"
#include "UI/Icons.h"

namespace {

    ImFont* s_font = nullptr;
    ImFont* s_mono = nullptr;

    void LoadFonts() {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        // the font arrays are static data, the atlas must not free them
        ImFontConfig text;
        text.FontDataOwnedByAtlas = false;
        text.OversampleH = 2;
        text.OversampleV = 2;
        s_font = io.Fonts->AddFontFromMemoryTTF(gDroidSansFont, (int)sizeof(gDroidSansFont), Theme::FONT_BODY, &text);

        // icons are merged into the text font so they can be used inline (ICON_FA_* in any label)
        ImFontConfig icons;
        icons.FontDataOwnedByAtlas = false;
        icons.MergeMode = true;
        icons.PixelSnapH = true;
        icons.GlyphMinAdvanceX = Theme::FONT_BODY; // fixed width so icon columns line up
        icons.GlyphOffset = ImVec2(0.0f, 0.5f);
        static const ImWchar ICON_RANGES[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
        icons.GlyphRanges = ICON_RANGES;
        io.Fonts->AddFontFromMemoryTTF(gFontAwesomeIconFont, (int)sizeof(gFontAwesomeIconFont), Theme::FONT_BODY * 0.9f,
                                       &icons);

        // ImGui's embedded scalable monospace font
        ImFontConfig mono;
        mono.SizePixels = Theme::FONT_BODY;
        s_mono = io.Fonts->AddFontDefaultVector(&mono);

        io.FontDefault = s_font;
    }

    void ApplyColors() {
        using namespace Theme;
        ImVec4* c = ImGui::GetStyle().Colors;

        c[ImGuiCol_Text] = Text;
        c[ImGuiCol_TextDisabled] = TextFaint;
        c[ImGuiCol_WindowBg] = Background;
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = Hex(0x171B21);
        c[ImGuiCol_Border] = Border;
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        c[ImGuiCol_FrameBg] = Surface2;
        c[ImGuiCol_FrameBgHovered] = Surface3;
        c[ImGuiCol_FrameBgActive] = Hex(0x29313D);

        c[ImGuiCol_TitleBg] = Background;
        c[ImGuiCol_TitleBgActive] = Background;
        c[ImGuiCol_TitleBgCollapsed] = Background;
        c[ImGuiCol_MenuBarBg] = Sidebar;

        c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab] = Hex(0x2A313C);
        c[ImGuiCol_ScrollbarGrabHovered] = Hex(0x37404D);
        c[ImGuiCol_ScrollbarGrabActive] = Hex(0x434D5C);

        c[ImGuiCol_CheckMark] = Accent;
        c[ImGuiCol_SliderGrab] = Accent;
        c[ImGuiCol_SliderGrabActive] = AccentStrong;

        c[ImGuiCol_Button] = Surface2;
        c[ImGuiCol_ButtonHovered] = Surface3;
        c[ImGuiCol_ButtonActive] = Hex(0x29313D);

        c[ImGuiCol_Header] = WithAlpha(Accent, 0.14f);
        c[ImGuiCol_HeaderHovered] = Hex(0xFFFFFF, 0.05f);
        c[ImGuiCol_HeaderActive] = WithAlpha(Accent, 0.20f);

        c[ImGuiCol_Separator] = Border;
        c[ImGuiCol_SeparatorHovered] = WithAlpha(Accent, 0.6f);
        c[ImGuiCol_SeparatorActive] = Accent;

        c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered] = WithAlpha(Accent, 0.5f);
        c[ImGuiCol_ResizeGripActive] = Accent;

        c[ImGuiCol_Tab] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TabHovered] = Surface3;
        c[ImGuiCol_TabSelected] = Surface2;
        c[ImGuiCol_TabSelectedOverline] = Accent;
        c[ImGuiCol_TabDimmed] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TabDimmedSelected] = Surface2;

        c[ImGuiCol_TableHeaderBg] = Surface;
        c[ImGuiCol_TableBorderStrong] = Border;
        c[ImGuiCol_TableBorderLight] = Hex(0x1D232B);
        c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = Hex(0xFFFFFF, 0.018f);

        c[ImGuiCol_TextSelectedBg] = WithAlpha(Accent, 0.28f);
        c[ImGuiCol_TextLink] = Accent;
        c[ImGuiCol_DragDropTarget] = Accent;
        c[ImGuiCol_NavCursor] = Accent;
        c[ImGuiCol_ModalWindowDimBg] = Hex(0x000000, 0.55f);
    }

}

void Theme::Apply(float dpiScale) {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();

    style.WindowPadding = ImVec2(0, 0);
    style.WindowBorderSize = 0.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = CARD_ROUNDING;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 8.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(10, 7);
    style.FrameRounding = 7.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.CellPadding = ImVec2(10, 6);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.TabBorderSize = 0.0f;
    style.SeparatorTextBorderSize = 1.0f;
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    ApplyColors();

    style.ScaleAllSizes(dpiScale);
    style.FontSizeBase = FONT_BODY;
    style.FontScaleDpi = dpiScale;

    LoadFonts();
}

void Theme::PushFontSize(float size) {
    ImGui::PushFont(s_font, size);
}

void Theme::PushMono(float size) {
    ImGui::PushFont(s_mono ? s_mono : s_font, size);
}
