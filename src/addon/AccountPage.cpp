#include <algorithm>

#include "Host.h"
#include "Ui.h"
#include "core/Rules.h"
#include "core/Text.h"

namespace wc::ui {

    using Loc::T;

    namespace {
        const int kSizes[] = {60, 75, 90, 100, 110, 125, 150, 175, 200};
    }

    /// the page of the account: who is linked, the link with a code, the account public events are signed up with, and the language and size
    void App::AccountPage() {
        ImGui::BeginChild("account", ImVec2(0, 0), false);

        BeginCard("who", Palette::Gold);
        Kicker(T("Account"));
        if (!m_api->Linked()) {
            Wrapped(T("This module is not linked to a Wondrous Compass account.\n\nOpen the Wondrous Compass app, sign in, go to Settings and press 'Get a code' under Blish HUD. Type the code here. It works once, for ten minutes."),
                    Palette::Text);
        } else if (!m_me) {
            Wrapped(T("Linked."), Palette::Text);
        } else {
            std::string guilds, accounts;
            for (const auto& g : m_me->guilds) {
                guilds += (guilds.empty() ? "" : ", ") + g.Label() + (g.CanCreate() ? " (" + EventRules::RankLabel(g.role) + ")" : "");
            }
            for (const auto& a : m_me->accounts) accounts += (accounts.empty() ? "" : ", ") + a.name;
            if (guilds.empty()) guilds = T("none");
            if (accounts.empty()) accounts = T("none");
            Wrapped(T("Linked as {0}.\n\nGame accounts: {1}\nGuilds: {2}\n\nOnly leaders and officers can make guild events.", {m_me->name, accounts, guilds}),
                    Palette::Text);
        }
        EndCard();

        BeginCard("link", Palette::Gold);
        Kicker(T("Link with a code"));
        Small(T("Get the code in the app or on the website: Settings, Blish HUD, Get a code."), Palette::Muted);
        Input("##code", m_code, "ABCD-EFGH", S(240));
        ImGui::SameLine();
        if (Button(T("Link"), Kind::Primary, !m_linking, S(130))) LinkNow();
        ImGui::SameLine();
        if (Button(T("Unlink"), Kind::Secondary, m_api->Linked() && !m_linking, S(130))) {
            Unlink();
            m_linkStatus.Clear();
        }
        m_linkStatus.Draw();
        EndCard();

        BeginCard("public", Palette::Gold);
        Kicker(T("Public events"));
        Small(T("The game account that signs up when you press Sign up on a public event. Automatic takes the first account of the region of the event."),
              Palette::Muted);
        std::vector<std::string> names{T("Automatic")};
        int pick = 0;
        if (m_me) {
            for (size_t i = 0; i < m_me->accounts.size(); ++i) {
                names.push_back(m_me->accounts[i].name);
                if (m_me->accounts[i].id == m_settings.defaultAccountId) pick = static_cast<int>(i) + 1;
            }
        }
        if (Combo("##account", names, pick, S(320))) {
            m_settings.defaultAccountId = pick == 0 ? "" : m_me->accounts[static_cast<size_t>(pick - 1)].id;
            SaveSettings();
        }
        EndCard();

        BeginCard("window", Palette::Gold);
        Kicker(T("Window and language"));
        LanguageAndSize(false);
        EndCard();

        ImGui::EndChild();
    }

    /// the language and the size of the window, which the options of the addon in Nexus also have
    void App::LanguageAndSize(bool inOptions) {
        float label = inOptions ? 220.0f : S(240);
        std::vector<std::string> names{T("Automatic")};
        std::vector<std::string> codes{"auto"};
        for (const auto& c : Loc::Codes()) {
            names.push_back(Loc::NativeName(c));
            codes.push_back(c);
        }
        int language = static_cast<int>(std::find(codes.begin(), codes.end(), m_settings.language) - codes.begin());
        FormLabel(T("Language"), label);
        if (Combo("##uilanguage", names, language, inOptions ? 220.0f : S(240))) {
            m_settings.language = codes[static_cast<size_t>(language)];
            SaveSettings();
            ApplyLanguage();
        }
        if (inOptions) ImGui::TextDisabled("%s", T("Auto follows the language of Nexus (English, German, Spanish, French, Portuguese, Russian or Chinese; anything else is English)."));

        std::vector<std::string> sizes;
        int size = 3;
        for (size_t i = 0; i < sizeof(kSizes) / sizeof(kSizes[0]); ++i) {
            sizes.push_back(std::to_string(kSizes[i]) + " %");
            if (kSizes[i] == m_settings.windowSize) size = static_cast<int>(i);
        }
        FormLabel(T("Window size (%)"), label);
        if (Combo("##uisize", sizes, size, inOptions ? 220.0f : S(240))) {
            m_settings.windowSize = kSizes[size];
            SaveSettings();
        }
        if (inOptions) ImGui::TextDisabled("%s", T("How big the window is drawn. 100 is the size of the windows of the game's interface."));
    }

    void App::LinkNow() {
        std::string code = Trimmed(m_code);
        if (code.size() < 8) {
            m_linkStatus.Set(T("Type the code from the app."), Palette::Red);
            return;
        }
        m_linking = true;
        m_linkStatus.Set(T("Linking..."), Palette::Muted);
        auto api = m_api;
        m_worker.Run<LinkResult>(
            [api, code]() { return api->Link(code, "Nexus"); },
            [this](LinkResult result) {
                m_linking = false;
                if (!result.Ok()) {
                    m_linkStatus.Set(ErrorText::ForLink(result.error), Palette::Red);
                    return;
                }
                if (!m_store.Save(result.token)) {
                    m_linkStatus.Set(T("Linked, but the token could not be saved. You will have to link again next time."), Palette::Red);
                } else {
                    m_linkStatus.Set(T("Linked as {0}.", {result.name}), Palette::Green);
                }
                m_code.clear();
                m_polledAt = 0;
                Refresh(true);
                Show(Page::Public);
            },
            [this](const ApiError& e) {
                m_linking = false;
                m_linkStatus.Set(e.code == "unknown" ? ErrorText::ForUnknown() : ErrorText::For(e), Palette::Red);
            });
    }
}
