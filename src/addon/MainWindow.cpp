#include <algorithm>
#include <ctime>

#include "Host.h"
#include "Ui.h"
#include "core/Rules.h"
#include "core/Text.h"

namespace wc::ui {

    using Loc::T;

    namespace {
        Moment Now() { return std::time(nullptr); }
        constexpr float kWidth = 880.0f;
        constexpr float kHeight = 660.0f;
    }

    App::App(std::shared_ptr<CodexApi> api, Worker& worker, TokenStore store, std::filesystem::path folder)
        : m_api(std::move(api)), m_worker(worker), m_store(std::move(store)), m_folder(std::move(folder)) {
        m_settings = Settings::Load(m_folder);
        ApplyLanguage();
        m_page = m_api->Linked() ? Page::Public : Page::Account;
    }

    void App::SaveSettings() {
        m_settings.Clamp();
        if (!m_settings.Save(m_folder)) Host::Warn("the settings could not be saved");
    }

    /// the language that is chosen, or the one of Nexus. the words of the window follow at once
    void App::ApplyLanguage() {
        std::string code = m_settings.language == "auto" ? Host::HostLanguage() : m_settings.language;
        if (code == m_language) return;
        m_language = code;
        Loc::Use(code);
    }

    std::string App::Profession() const { return Host::Profession(); }

    void App::Toggle() {
        m_visible = !m_visible;
        if (m_visible) Refresh(false);
    }

    void App::Frame() {
        m_worker.Drain();
        Moment now = Now();

        // Nexus follows the language of the game, which can change while the addon runs
        if (now != m_languageCheckedAt) {
            m_languageCheckedAt = now;
            ApplyLanguage();
        }

        if (m_visible) {
            // the window reads again by itself while it is open
            if (m_api->Linked() && !m_loading && now - m_loadedAt > 60 * std::max(1, m_settings.refreshMinutes)) Refresh(true);
            Window();
        }
        Reminders(now);
    }

    /// the events of the player are read now and then, not at each frame, and what begins soon is told from what was read, every few seconds
    void App::Reminders(Moment now) {
        if (!m_settings.reminders || !m_api->Linked()) return;
        if (!m_polling && now - m_polledAt > 60 * std::max(1, m_settings.refreshMinutes)) {
            m_polling = true;
            m_polledAt = now;
            auto api = m_api;
            m_worker.Run<std::vector<EventInfo>>(
                [api]() {
                    auto open = api->MyOpenEvents();
                    auto guild = api->GuildEvents();
                    open.insert(open.end(), guild.begin(), guild.end());
                    return open;
                },
                [this](std::vector<EventInfo> events) {
                    m_polling = false;
                    m_remindable = std::move(events);
                },
                [this](const ApiError& e) {
                    m_polling = false;
                    if (e.IsInvalidToken()) Unlink();
                });
        }
        if (now - m_checkedAt >= 5) {
            m_checkedAt = now;
            for (const auto& due : m_planner.Due(m_remindable, now, m_settings.reminderMinutes)) Host::Alert(ReminderPlanner::Message(due, now));
            m_planner.Forget();
        }
    }

    void App::Show(Page page) {
        if (page != Page::Account && !m_api->Linked()) page = Page::Account;
        m_page = page;
        if (page != Page::Details) m_back = page;
        Moment now = Now();
        if (page == Page::Find) {
            m_guildOpen = false;
            if (now - m_foundAt > 30) LoadGuilds();
        }
        if (page == Page::Recruit) {
            m_editing.reset();
            if (now - m_myGuildsAt > 60) LoadMyGuilds();
        }
        if ((page == Page::Public || page == Page::Guild || page == Page::Mine) && now - m_loadedAt > 30) Refresh(false);
    }

    /// reads everything the lists need. [force] reads again even though it was done a moment ago
    void App::Refresh(bool force) {
        if (!m_api->Linked() || m_loading) return;
        Moment now = Now();
        if (!force && now - m_loadedAt < 30) return;
        m_loading = true;
        m_status.Set(T("Loading events..."), Palette::Muted);
        EventFilter filter = m_publicFilter;
        int limit = m_publicLimit;
        auto api = m_api;
        struct Lists {
            MeInfo me;
            std::vector<EventInfo> open, guild, mine;
        };
        m_worker.Run<Lists>(
            [api, filter, limit]() {
                Lists r;
                r.me = api->Me();
                r.open = api->OpenEvents(filter, limit, 0);
                r.guild = api->GuildEvents();
                r.mine = api->MyOpenEvents();
                return r;
            },
            [this](Lists r) {
                m_loading = false;
                m_loadedAt = Now();
                m_me = std::move(r.me);
                m_open = std::move(r.open);
                m_guild = std::move(r.guild);
                m_mine = std::move(r.mine);
                m_status.Clear();
                // a guild chosen in the form that the player can no longer make events for is dropped
                if (m_draft.IsGuild() && !m_draft.IsEdit()) {
                    auto guilds = CreatableGuilds();
                    if (std::none_of(guilds.begin(), guilds.end(), [this](const GuildInfo& g) { return g.guildId == m_draft.guildId; })) m_draft.guildId.clear();
                }
            },
            [this](const ApiError& e) {
                m_loading = false;
                // a list that could not be read is tried again at the next turn, not at the next frame
                m_loadedAt = Now();
                Failed(e);
            });
    }

    void App::Failed(const ApiError& e) {
        if (e.IsInvalidToken()) Unlink();
        m_status.Set(e.code == "unknown" ? ErrorText::ForUnknown() : ErrorText::For(e), Palette::Red);
    }

    /// the token was cut, or the player cut it: the data of the player is left and a new code is asked for
    void App::Unlink() {
        m_store.Clear();
        m_api->SetToken("");
        m_me.reset();
        m_open.clear();
        m_guild.clear();
        m_mine.clear();
        m_remindable.clear();
        m_myGuilds.clear();
        m_myGuildsAt = 0;
        m_editing.reset();
        m_details.reset();
        m_draft = EventDraft();
        m_page = Page::Account;
    }

    void App::Window() {
        SetScale(m_settings.windowSize / 100.0f);
        PushTheme();
        ImGui::SetNextWindowSize(ImVec2(S(kWidth), S(kHeight)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(S(640), S(420)), ImVec2(FLT_MAX, FLT_MAX));
        // the name is the one Nexus is told to close on escape, so it does not change with the language
        if (!ImGui::Begin(kWindowName, &m_visible, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            PopTheme();
            return;
        }
        ImGui::SetWindowFontScale(m_settings.windowSize / 100.0f);
        if (Host::BodyFont()) ImGui::PushFont(Host::BodyFont());

        // the pages, as tabs with a line under them; the page of an event keeps the tab of the list it was opened from lit
        Page lit = m_page == Page::Details ? m_back : m_page;
        const std::pair<Page, const char*> tabs[] = {{Page::Public, T("Public")}, {Page::Guild, T("Guild")},      {Page::Mine, T("My events")},
                                                     {Page::Create, T("Create")}, {Page::Find, T("Find guild")}, {Page::Recruit, T("Recruit")},
                                                     {Page::Account, T("Account")}};
        ImDrawList* draw = ImGui::GetWindowDrawList();
        for (const auto& [page, label] : tabs) {
            bool active = page == lit;
            ImGui::PushID(static_cast<int>(page));
            ImGui::PushStyleColor(ImGuiCol_Text, active ? Palette::GoldSoft : Palette::TextSoft);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Palette::Surface2);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Palette::Surface3);
            if (Host::StrongFont()) ImGui::PushFont(Host::StrongFont());
            if (ImGui::Button(label)) Show(page);
            if (Host::StrongFont()) ImGui::PopFont();
            ImGui::PopStyleColor(4);
            if (active) {
                ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                draw->AddRectFilled(ImVec2(a.x + S(6), b.y - S(2)), ImVec2(b.x - S(6), b.y), Palette::U32(Palette::Gold));
            }
            ImGui::PopID();
            ImGui::SameLine();
        }
        float refreshWidth = ImGui::CalcTextSize(T("Refresh")).x + S(28);
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - refreshWidth));
        if (Button(T("Refresh"), Kind::Quiet, m_api->Linked() && !m_loading)) {
            Refresh(true);
            if (m_page == Page::Find) LoadGuilds();
            if (m_page == Page::Recruit && !m_editing) LoadMyGuilds();
        }
        ImGui::Separator();

        float footer = ImGui::GetTextLineHeightWithSpacing() + S(6);
        ImGui::BeginChild("page", ImVec2(0, -footer), false);
        switch (m_page) {
            case Page::Public: PublicPage(); break;
            case Page::Guild: GuildPage(); break;
            case Page::Mine: MinePage(); break;
            case Page::Create: CreatePage(); break;
            case Page::Find: FindPage(); break;
            case Page::Recruit: ManagePage(); break;
            case Page::Account: AccountPage(); break;
            case Page::Details: DetailsPage(); break;
        }
        ImGui::EndChild();
        m_status.Draw();

        if (Host::BodyFont()) ImGui::PopFont();
        ImGui::End();
        PopTheme();
    }

    void App::Options() {
        SetScale(1.0f);
        ImGui::TextUnformatted("Wondrous Compass");
        ImGui::Separator();
        LanguageAndSize(true);
        bool reminders = m_settings.reminders;
        if (ImGui::Checkbox(T("Remind me before events"), &reminders)) {
            m_settings.reminders = reminders;
            SaveSettings();
        }
        ImGui::TextDisabled("%s", T("A notice on the screen shortly before an event that you signed up for, or made, begins."));
        int minutes = m_settings.reminderMinutes;
        ImGui::SetNextItemWidth(220);
        // a slider changes at each step of the drag: the value follows at once, the file is written once it is let go
        if (ImGui::SliderInt(T("Minutes before"), &minutes, 1, 60)) m_settings.reminderMinutes = minutes;
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
        ImGui::TextDisabled("%s", T("How long before the start the notice comes."));
        int refresh = m_settings.refreshMinutes;
        ImGui::SetNextItemWidth(220);
        if (ImGui::SliderInt(T("Read the events every (minutes)"), &refresh, 1, 30)) m_settings.refreshMinutes = refresh;
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
        ImGui::TextDisabled("%s", T("How often the events are read again, for the window while it is open and for the reminders."));
        ImGui::TextDisabled("%s", T("A key that opens and closes the window of Wondrous Compass."));
    }
}
