#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Widgets.h"
#include "Worker.h"
#include "core/Api.h"
#include "core/Drafts.h"
#include "core/Models.h"
#include "core/Support.h"

namespace wc::ui {

    /// the name of the window, by which Nexus closes it on escape
    constexpr const char* kWindowName = "Wondrous Compass";

    enum class Page { Public, Guild, Mine, Create, Find, Recruit, Account, Details };

    /// the window of the addon: public events, guild events, the ones the player is in, making one, finding and writing guild pages, and the
    /// link. everything it holds is read and changed on the thread that draws; the calls to the server go through the worker
    class App {
    public:
        App(std::shared_ptr<CodexApi> api, Worker& worker, TokenStore store, std::filesystem::path folder);

        void Toggle();
        bool Visible() const { return m_visible; }
        bool* VisibleFlag() { return &m_visible; }

        /// once a frame: the window when it is open, the reminders, and what the worker has finished
        void Frame();

        /// the settings of the addon in the options of Nexus
        void Options();

#ifdef WC_UI_TESTS
        friend struct UiTestAccess;
#endif

    private:
        // ---- MainWindow.cpp
        void Window();
        void Show(Page page);
        void Refresh(bool force);
        void Failed(const ApiError& e);
        void Unlink();
        void ApplyLanguage();
        void Reminders(Moment now);
        void SaveSettings();
        std::string Profession() const;

        // ---- EventPages.cpp
        void PublicPage();
        void GuildPage();
        void MinePage();
        void EventList(const char* id, const std::vector<EventInfo>& events, const std::string& empty, bool canLoadMore);
        void EventCard(const EventInfo& e, Moment now);
        void Join(const EventInfo& e, const std::string& role, const std::string& status);
        void Leave(const EventInfo& e);
        void Act(const std::string& busyKey, std::function<void()> call);
        void OpenDetails(const EventInfo& e);
        void DetailsPage();

        // ---- CreatePage.cpp
        void CreatePage();
        void EditEvent(const EventInfo& e);
        void StopEditing();
        void CreateNow();
        void SaveEdit();
        void RemoveEvent();
        void StopSeries();
        std::vector<GuildInfo> CreatableGuilds() const;
        std::vector<GuildInfo> Organizers(const std::string& guildId) const;

        // ---- RecruitPages.cpp
        void FindPage();
        void LoadGuilds();
        void OpenGuild(const RecruitCard& c);
        void GuildDetail();
        void ManagePage();
        void LoadMyGuilds();
        void OpenEditor(const MyRecruitGuild& g);
        void Editor();
        void LoadRecruiters();
        void SavePage(bool show);
        void Unpublish();
        void DeletePage();

        // ---- AccountPage.cpp
        void AccountPage();
        void LinkNow();
        void LanguageAndSize(bool inOptions);

        std::shared_ptr<CodexApi> m_api;
        Worker& m_worker;
        TokenStore m_store;
        std::filesystem::path m_folder;
        Settings m_settings;
        ReminderPlanner m_planner;

        bool m_visible = false;
        Page m_page = Page::Public;
        Page m_back = Page::Public;
        std::string m_language;          // the language the words are in now
        Moment m_languageCheckedAt = 0;
        Status m_status;

        // what the server said
        std::optional<MeInfo> m_me;
        std::vector<EventInfo> m_open, m_guild, m_mine;
        bool m_loading = false;
        Moment m_loadedAt = 0;
        std::set<std::string> m_busy;    // the buttons whose call is under way

        // the reminders, read now and then on their own
        std::vector<EventInfo> m_remindable;
        Moment m_polledAt = 0, m_checkedAt = 0;
        bool m_polling = false;

        // the lists
        EventFilter m_publicFilter;
        int m_publicLimit = 50;
        int m_publicActivity = 0, m_publicLanguage = 0, m_publicRegion = 0, m_publicTag = 0;
        int m_guildActivity = 0, m_guildPick = 0;
        std::map<std::string, int> m_rolePick;  // the role chosen on the card of each event

        // the page of one event
        std::optional<EventInfo> m_details;
        std::vector<RosterEntry> m_roster;
        bool m_rosterLoading = false;
        Status m_rosterStatus;
        std::set<std::string> m_flex;
        Status m_flexStatus, m_reportStatus;
        int m_reportReason = 0;
        bool m_reported = false;
        std::string m_copied;

        // making and changing an event
        EventDraft m_draft;
        bool m_confirmDelete = false;
        bool m_creating = false;
        Status m_createStatus;

        // finding a guild
        RecruitFilter m_findFilter;
        int m_findLanguage = 0, m_findRegion = 0, m_findTag = 0;
        RecruitList m_found;
        bool m_finding = false;
        Moment m_foundAt = 0;
        Status m_findStatus;
        std::optional<wc::RecruitPage> m_guildPage;
        bool m_guildOpen = false;
        Status m_guildStatus, m_guildReportStatus;
        int m_guildReportReason = 0;

        // writing the page of a guild
        std::vector<MyRecruitGuild> m_myGuilds;
        bool m_myGuildsLoading = false;
        Moment m_myGuildsAt = 0;
        Status m_myGuildsStatus;
        std::optional<MyRecruitGuild> m_editing;
        std::optional<wc::RecruitPage> m_recruitPage;
        RecruitDraft m_pageDraft;
        bool m_pageLoading = false, m_pageBusy = false, m_confirmPageDelete = false;
        Status m_pageStatus;
        std::vector<Recruiter> m_recruiters, m_members;
        int m_allowPick = 0;

        // the link
        std::string m_code;
        Status m_linkStatus;
        bool m_linking = false;
    };
}
