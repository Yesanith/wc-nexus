#include "Widgets.h"

#include <algorithm>
#include <cctype>

#include "Host.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

namespace wc::ui {

    namespace Palette {
        static ImVec4 Rgb(int r, int g, int b) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f); }

        const ImVec4 Bg = Rgb(0x14, 0x10, 0x0B);
        const ImVec4 Chrome = Rgb(0x0E, 0x0B, 0x07);
        const ImVec4 Surface = Rgb(0x1C, 0x16, 0x0E);
        const ImVec4 Surface2 = Rgb(0x27, 0x1E, 0x13);
        const ImVec4 Surface3 = Rgb(0x32, 0x27, 0x1A);
        const ImVec4 Line = Rgb(0x3E, 0x32, 0x21);
        const ImVec4 LineSoft = Rgb(0x2B, 0x22, 0x16);
        const ImVec4 Text = Rgb(0xEF, 0xE3, 0xC8);
        const ImVec4 TextSoft = Rgb(0xCD, 0xBF, 0x9F);
        const ImVec4 Muted = Rgb(0x9C, 0x8C, 0x6E);
        const ImVec4 Hint = Rgb(0x7E, 0x70, 0x5A);
        const ImVec4 Gold = Rgb(0xD9, 0xA8, 0x4E);
        const ImVec4 GoldSoft = Rgb(0xF0, 0xD0, 0x8A);
        const ImVec4 GoldDeep = Rgb(0xB8, 0x86, 0x32);
        const ImVec4 OnGold = Rgb(0x25, 0x1C, 0x12);
        const ImVec4 Ember = Rgb(0xE0, 0x80, 0x3F);
        const ImVec4 Blue = Rgb(0x78, 0xAE, 0xE6);
        const ImVec4 Green = Rgb(0x6F, 0xD0, 0x9A);
        const ImVec4 Red = Rgb(0xEC, 0x8A, 0x82);

        ImVec4 Activity(const std::string& a) {
            if (a == "raid") return Ember;
            if (a == "strike") return Red;
            if (a == "fractal") return Blue;
            if (a == "worldboss") return Gold;
            if (a == "meta") return Green;
            if (a == "wvw") return Rgb(0xC9, 0x7B, 0xD6);
            if (a == "pvp") return Rgb(0xE6, 0x6F, 0x8E);
            if (a == "dungeon") return Rgb(0x7F, 0xCF, 0xCF);
            return Muted;
        }

        ImU32 U32(const ImVec4& c, float alpha) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f); }
    }

    namespace {
        float g_scale = 1.0f;

        struct CardFrame {
            ImVec2 start;
            ImVec4 accent;
            bool hoverable;
            float width;
        };
        std::vector<CardFrame> g_cards;

        void PushFont(ImFont* font) { ImGui::PushFont(font ? font : ImGui::GetFont()); }
    }

    void SetScale(float scale) { g_scale = std::max(0.5f, std::min(2.5f, scale)); }
    float S(float v) { return v * g_scale; }

    void PushTheme() {
        using namespace Palette;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Bg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, Surface);
        ImGui::PushStyleColor(ImGuiCol_Border, Line);
        ImGui::PushStyleColor(ImGuiCol_Text, Palette::Text);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, Hint);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, Chrome);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, Chrome);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, Chrome);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Surface3);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Surface3);
        ImGui::PushStyleColor(ImGuiCol_Button, Surface2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Surface3);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Line);
        ImGui::PushStyleColor(ImGuiCol_Header, Surface3);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Line);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, Line);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, Gold);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, Gold);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, GoldSoft);
        ImGui::PushStyleColor(ImGuiCol_Tab, Chrome);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, Surface3);
        ImGui::PushStyleColor(ImGuiCol_TabActive, Surface2);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused, Chrome);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, Surface2);
        ImGui::PushStyleColor(ImGuiCol_Separator, Line);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, Bg);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, Surface3);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, Line);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, Muted);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(Gold.x, Gold.y, Gold.z, 0.35f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(7));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(7));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, S(7));
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, S(7));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, S(7));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(16), S(12)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(10), S(7)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8), S(8)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    }

    void PopTheme() {
        ImGui::PopStyleVar(11);
        ImGui::PopStyleColor(32);
    }

    bool Button(const std::string& label, Kind kind, bool enabled, float width) {
        using namespace Palette;
        ImVec4 bg = Surface2, hover = Surface3, text = Palette::Text;
        switch (kind) {
            case Kind::Primary:
                bg = Gold;
                hover = GoldSoft;
                text = OnGold;
                break;
            case Kind::Danger:
                bg = Mix(Surface2, Red, 0.35f);
                hover = Mix(Surface2, Red, 0.55f);
                break;
            case Kind::Quiet:
                bg = ImVec4(0, 0, 0, 0);
                hover = Surface2;
                text = TextSoft;
                break;
            default:
                break;
        }
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        if (!enabled) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
        }
        PushFont(Host::StrongFont());
        if (width > 0) width = std::max(width, ImGui::CalcTextSize(label.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2);
        bool pressed = ImGui::Button(label.c_str(), ImVec2(width, 0));
        ImGui::PopFont();
        if (!enabled) {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
        }
        ImGui::PopStyleColor(4);
        return pressed && enabled;
    }

    void Chip(const std::string& text, const ImVec4& tone) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 size = ImGui::CalcTextSize(text.c_str());
        float padX = S(9), padY = S(3);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 q(p.x + size.x + 2 * padX, p.y + size.y + 2 * padY);
        draw->AddRectFilled(p, q, Palette::U32(tone, 0.16f), S(99));
        draw->AddRect(p, q, Palette::U32(tone, 0.45f), S(99));
        draw->AddText(ImVec2(p.x + padX, p.y + padY), Palette::U32(tone), text.c_str());
        ImGui::Dummy(ImVec2(q.x - p.x, q.y - p.y));
    }

    void Kicker(const std::string& text) {
        std::string up = text;
        // capitals of the letters of ASCII only: other scripts are left as they are written
        for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        PushFont(Host::StrongFont());
        ImGui::PushStyleColor(ImGuiCol_Text, Palette::Gold);
        ImGui::TextUnformatted(up.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void Text(const std::string& text, const ImVec4& color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopStyleColor();
    }

    void Wrapped(const std::string& text, const ImVec4& color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    void Small(const std::string& text, const ImVec4& color) {
        ImGui::SetWindowFontScale(0.9f * g_scale);
        Wrapped(text, color);
        ImGui::SetWindowFontScale(g_scale);
    }

    void Title(const std::string& text, const ImVec4& color) {
        PushFont(Host::TitleFont());
        Text(text, color);
        ImGui::PopFont();
    }

    void Strong(const std::string& text, const ImVec4& color) {
        PushFont(Host::StrongFont());
        Text(text, color);
        ImGui::PopFont();
    }

    bool Combo(const char* id, const std::vector<std::string>& items, int& index, float width) {
        if (items.empty()) return false;
        index = std::max(0, std::min(static_cast<int>(items.size()) - 1, index));
        ImGui::SetNextItemWidth(width);
        bool changed = false;
        if (ImGui::BeginCombo(id, items[static_cast<size_t>(index)].c_str())) {
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                bool selected = i == index;
                if (ImGui::Selectable(items[static_cast<size_t>(i)].c_str(), selected)) {
                    changed = i != index;
                    index = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool Input(const char* id, std::string& value, const std::string& hint, float width) {
        ImGui::SetNextItemWidth(width);
        return ImGui::InputTextWithHint(id, hint.c_str(), &value);
    }

    bool Memo(const char* id, std::string& value, float width, float height) {
        return ImGui::InputTextMultiline(id, &value, ImVec2(width, height));
    }

    bool Check(const std::string& label, bool& value) { return ImGui::Checkbox(label.c_str(), &value); }

    void BeginCard(const char* id, const ImVec4& accent, bool hoverable) {
        ImGui::PushID(id);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->ChannelsSplit(2);
        draw->ChannelsSetCurrent(1);
        ImVec2 start = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        g_cards.push_back({start, accent, hoverable, width});
        ImGui::SetCursorScreenPos(ImVec2(start.x + S(18), start.y + S(14)));
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - S(36));
    }

    bool EndCard() {
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        CardFrame f = g_cards.back();
        g_cards.pop_back();
        ImVec2 groupMax = ImGui::GetItemRectMax();
        ImVec2 end(f.start.x + f.width, groupMax.y + S(14));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        bool hovered = ImGui::IsMouseHoveringRect(f.start, end) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        bool clicked = f.hoverable && hovered && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
        draw->ChannelsSetCurrent(0);
        ImVec4 bg = f.hoverable && hovered ? Palette::Surface2 : Palette::Surface;
        draw->AddRectFilled(f.start, end, Palette::U32(bg), S(10));
        draw->AddRect(f.start, end, Palette::U32(hovered && f.hoverable ? Palette::Line : Palette::LineSoft), S(10));
        draw->AddRectFilled(ImVec2(f.start.x, f.start.y + S(10)), ImVec2(f.start.x + S(4), end.y - S(10)), Palette::U32(f.accent), S(2));
        draw->ChannelsMerge();
        ImGui::SetCursorScreenPos(f.start);
        ImGui::Dummy(ImVec2(f.width, end.y - f.start.y));
        ImGui::PopID();
        return clicked;
    }

    float ButtonWidth(const std::string& label, float least) {
        PushFont(Host::StrongFont());
        float w = std::max(least, ImGui::CalcTextSize(label.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2);
        ImGui::PopFont();
        return w;
    }

    void FormLabel(const std::string& label, float labelWidth) {
        ImGui::AlignTextToFramePadding();
        Text(label, Palette::TextSoft);
        ImGui::SameLine(labelWidth);
    }

    void Status::Draw() const {
        if (!text.empty()) Small(text, color);
    }

    void Copy(const std::string& text) { ImGui::SetClipboardText(text.c_str()); }
}
