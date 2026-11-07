#pragma once

#include <string>
#include <vector>

#include "imgui.h"

namespace wc::ui {

    /// the night theme of the app and the website
    namespace Palette {
        extern const ImVec4 Bg, Chrome, Surface, Surface2, Surface3, Line, LineSoft, Text, TextSoft, Muted, Hint, Gold, GoldSoft, GoldDeep, OnGold, Ember,
            Blue, Green, Red;

        /// the colour of a kind of event
        ImVec4 Activity(const std::string& activity);
        ImU32 U32(const ImVec4& c, float alpha = 1.0f);
        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float amount);
    }

    enum class Kind { Primary, Secondary, Quiet, Danger };

    /// the sizes of the layout are for 100%; the window is drawn at the size the player chose
    void SetScale(float scale);
    float S(float v);

    /// the colours and the roundness of the window, pushed while it is drawn
    void PushTheme();
    void PopTheme();

    /// a button of the app. a button that is not enabled is drawn faint and does nothing
    bool Button(const std::string& label, Kind kind = Kind::Secondary, bool enabled = true, float width = 0.0f);

    /// how wide a button is drawn: [least], or more when its words need it
    float ButtonWidth(const std::string& label, float least);

    /// a small rounded label: what an event is, when, how many go. it is drawn where the cursor is, and the cursor goes on beside it
    void Chip(const std::string& text, const ImVec4& tone);

    /// the small heading of a card, in capitals and in gold
    void Kicker(const std::string& text);

    void Text(const std::string& text, const ImVec4& color);
    void Wrapped(const std::string& text, const ImVec4& color);
    void Small(const std::string& text, const ImVec4& color);
    void Title(const std::string& text, const ImVec4& color);
    void Strong(const std::string& text, const ImVec4& color);

    /// a choice of one among [items]: true when it changed
    bool Combo(const char* id, const std::vector<std::string>& items, int& index, float width);

    /// a line to type in: true when it changed
    bool Input(const char* id, std::string& value, const std::string& hint, float width);
    bool Memo(const char* id, std::string& value, float width, float height);
    bool Check(const std::string& label, bool& value);

    /// a card of the list: a surface with a coloured edge on the left, as high as what is drawn in it. Begin returns nothing to check: draw in
    /// it, and End says whether it was clicked where no button is
    void BeginCard(const char* id, const ImVec4& accent, bool hoverable = false);
    bool EndCard();

    /// a label in the column of labels of a form, and the cursor put where the field goes
    void FormLabel(const std::string& label, float labelWidth);

    /// what was done or went wrong, under a form or a list
    struct Status {
        std::string text;
        ImVec4 color = Palette::Muted;
        void Set(const std::string& t, const ImVec4& c) {
            text = t;
            color = c;
        }
        void Clear() { text.clear(); }
        void Draw() const;
    };

    /// copies a text to the clipboard of Windows
    void Copy(const std::string& text);
}
