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
        float LabelWidth() { return S(176); }

        int IndexOf(const std::vector<std::string>& items, const std::string& value) {
            auto it = std::find(items.begin(), items.end(), value);
            return it == items.end() ? 0 : static_cast<int>(it - items.begin());
        }

        /// how a guild is shown in the choice of where the event is made
        std::string GuildItem(const GuildInfo& g) { return T("Guild: {0}", {g.Label()}); }
    }

    std::vector<GuildInfo> App::CreatableGuilds() const {
        std::vector<GuildInfo> list;
        if (!m_me) return list;
        for (const auto& g : m_me->guilds) {
            if (!g.CanCreate()) continue;
            if (std::none_of(list.begin(), list.end(), [&](const GuildInfo& x) { return x.guildId == g.guildId; })) list.push_back(g);
        }
        return list;
    }

    /// the game accounts that organize for a guild: the ones that are in it with the rank
    std::vector<GuildInfo> App::Organizers(const std::string& guildId) const {
        std::vector<GuildInfo> list;
        if (!m_me) return list;
        for (const auto& g : m_me->guilds) {
            if (g.guildId == guildId && g.CanCreate()) list.push_back(g);
        }
        return list;
    }

    /// an event that is already there is changed in the same form: what it says now is what is on the screen
    void App::EditEvent(const EventInfo& e) {
        m_draft = EventDraft::FromEvent(e);
        m_confirmDelete = false;
        m_createStatus.Clear();
    }

    /// the form of a new event again, and the player is sent back to where they came from
    void App::StopEditing() {
        m_draft = EventDraft();
        m_confirmDelete = false;
        Show(Page::Mine);
    }

    /// the form of a new event, with what the website asks: where, title, activity, region, language, tags, map, time, length, weekly, roles
    /// and places, how the organizer comes, and notes
    void App::CreatePage() {
        ImGui::BeginChild("create", ImVec2(0, 0), false);
        auto guilds = CreatableGuilds();
        bool guild = m_draft.IsGuild();
        bool editing = m_draft.IsEdit();
        float field = std::min(S(400), ImGui::GetContentRegionAvail().x - LabelWidth() - S(60));

        if (editing) {
            ImGui::AlignTextToFramePadding();
            Title(guild ? T("Changing the guild event") : T("Changing the public event"), Palette::GoldSoft);
            float stop = ButtonWidth(T("Stop changing"), S(170));
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - stop - S(12));
            if (Button(T("Stop changing"), Kind::Secondary, true, stop)) {
                StopEditing();
                ImGui::EndChild();
                return;
            }
        }

        BeginCard("details", Palette::Gold);
        Kicker(T("Details"));

        // where
        if (!editing) {
            std::vector<std::string> targets{T("Public event")};
            for (const auto& g : guilds) targets.push_back(GuildItem(g));
            int where = 0;
            for (size_t i = 0; i < guilds.size(); ++i) {
                if (guilds[i].guildId == m_draft.guildId) where = static_cast<int>(i) + 1;
            }
            FormLabel(T("Where"), LabelWidth());
            if (Combo("##where", targets, where, field)) {
                m_draft.guildId = where == 0 ? "" : guilds[static_cast<size_t>(where - 1)].guildId;
                auto orgs = Organizers(m_draft.guildId);
                m_draft.accountId = orgs.empty() ? "" : orgs.front().accountId;
                const auto& allowed = where == 0 ? EventRules::OpenActivities : EventRules::GuildActivities;
                if (std::find(allowed.begin(), allowed.end(), m_draft.activity) == allowed.end()) m_draft.activity = "other";
                guild = m_draft.IsGuild();
            }
        }

        if (guild && !editing) {
            auto orgs = Organizers(m_draft.guildId);
            if (m_draft.accountId.empty() || std::none_of(orgs.begin(), orgs.end(), [&](const GuildInfo& o) { return o.accountId == m_draft.accountId; })) {
                m_draft.accountId = orgs.empty() ? "" : orgs.front().accountId;
            }
            FormLabel(T("Organize as"), LabelWidth());
            if (orgs.empty()) {
                Text(T("None of your accounts can organize for this guild."), Palette::Red);
            } else {
                std::vector<std::string> names;
                int pick = 0;
                for (size_t i = 0; i < orgs.size(); ++i) {
                    names.push_back(orgs[i].accountName);
                    if (orgs[i].accountId == m_draft.accountId) pick = static_cast<int>(i);
                }
                if (Combo("##organizer", names, pick, S(300))) m_draft.accountId = orgs[static_cast<size_t>(pick)].accountId;
            }
        }

        FormLabel(T("Title"), LabelWidth());
        Input("##title", m_draft.title, T("Fractals CM, Dragonstorm, ..."), field);

        const auto& activities = guild ? EventRules::GuildActivities : EventRules::OpenActivities;
        std::vector<std::string> activityLabels;
        for (const auto& a : activities) activityLabels.push_back(EventRules::ActivityLabel(a));
        int activity = IndexOf(activities, m_draft.activity);
        FormLabel(T("Activity"), LabelWidth());
        if (Combo("##activity", activityLabels, activity, S(200))) m_draft.activity = activities[static_cast<size_t>(activity)];

        if (!guild) {
            if (!editing) {
                // all regions, and each region the player has an account in
                std::vector<std::string> regions{"all"};
                if (m_me) {
                    for (const char* r : {"eu", "na"}) {
                        if (std::any_of(m_me->accounts.begin(), m_me->accounts.end(), [&](const AccountInfo& a) { return a.region == r; })) regions.push_back(r);
                    }
                }
                if (std::find(regions.begin(), regions.end(), m_draft.region) == regions.end()) m_draft.region = "all";
                std::vector<std::string> labels;
                for (const auto& r : regions) labels.push_back(EventRules::RegionLabel(r));
                int region = IndexOf(regions, m_draft.region);
                FormLabel(T("Region"), LabelWidth());
                if (Combo("##region", labels, region, S(200))) m_draft.region = regions[static_cast<size_t>(region)];
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                Text(T("who can sign up: accounts of that region"), Palette::Muted);
            }

            int language = IndexOf(EventRules::Languages, m_draft.language);
            FormLabel(T("Language"), LabelWidth());
            if (Combo("##language", EventRules::Languages, language, S(120))) m_draft.language = EventRules::Languages[static_cast<size_t>(language)];

            FormLabel(T("Tags (up to 3)"), LabelWidth());
            float tagsStart = ImGui::GetCursorPosX();
            float tagsLimit = ImGui::GetWindowContentRegionMax().x - S(40);
            bool firstTag = true;
            for (const auto& tag : EventRules::Tags) {
                std::string label = EventRules::TagLabel(tag);
                float width = ImGui::CalcTextSize(label.c_str()).x + ImGui::GetFrameHeight() + S(8);
                if (!firstTag) {
                    ImGui::SameLine(0, S(14));
                    if (ImGui::GetCursorPosX() + width > tagsLimit) {
                        ImGui::NewLine();
                        ImGui::SetCursorPosX(tagsStart);
                    }
                }
                firstTag = false;
                bool on = m_draft.tags.count(tag) > 0;
                if (Check(label, on)) m_draft.ToggleTag(tag, on);
            }

            FormLabel(T("Map id"), LabelWidth());
            Input("##map", m_draft.mapText, T("optional, number from the wiki"), S(220));
        }
        EndCard();

        BeginCard("when", Palette::Gold);
        Kicker(T("When"));
        FormLabel(T("When"), LabelWidth());
        Input("##when", m_draft.whenText, T("20:00 or +1d 20:00"), S(200));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        Text(T("your local time, also tomorrow 21:30 or 2026-12-24 19:00"), Palette::Muted);

        std::vector<std::string> lengths;
        int length = 0;
        for (size_t i = 0; i < EventRules::Durations.size(); ++i) {
            lengths.push_back(EventRules::DurationLabel(EventRules::Durations[i]));
            if (EventRules::Durations[i] == m_draft.durationMinutes) length = static_cast<int>(i);
        }
        FormLabel(T("Length"), LabelWidth());
        if (Combo("##length", lengths, length, S(140))) m_draft.durationMinutes = EventRules::Durations[static_cast<size_t>(length)];

        if (guild && !editing) {
            FormLabel(T("Repeat"), LabelWidth());
            Check(T("Every week at this time"), m_draft.weekly);
        }
        EndCard();

        BeginCard("roles", Palette::Gold);
        Kicker(T("Roles"));
        FormLabel(T("Roles"), LabelWidth());
        bool firstPreset = true;
        for (const auto& name : EventRules::PresetNames) {
            if (!firstPreset) ImGui::SameLine();
            firstPreset = false;
            ImGui::PushID(name.c_str());
            if (Button(EventRules::PresetLabel(name), Kind::Secondary, true, S(140))) m_draft.UsePreset(name);
            ImGui::PopID();
        }
        for (const auto& key : EventRules::RoleKeys) {
            auto it = m_draft.roles.find(key);
            int count = it == m_draft.roles.end() ? 0 : it->second;
            ImGui::PushID(key.c_str());
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + LabelWidth());
            ImGui::AlignTextToFramePadding();
            Text(EventRules::RoleLabel(key), count > 0 ? Palette::Text : Palette::Muted);
            ImGui::SameLine(LabelWidth() + S(150));
            if (Button("-", Kind::Secondary, count > 0, S(36))) m_draft.ChangeRole(key, -1);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            Strong(std::to_string(count), count > 0 ? Palette::GoldSoft : Palette::Muted);
            ImGui::SameLine(LabelWidth() + S(230));
            if (Button("+", Kind::Secondary, true, S(36))) m_draft.ChangeRole(key, 1);
            ImGui::PopID();
        }

        if (!m_draft.roles.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + LabelWidth());
            Small(T("Places: {0} (the places of the roles)", {std::to_string(m_draft.RoleTotal())}), Palette::Muted);
            if (!guild) {
                std::vector<std::string> roles;
                for (const auto& r : EventRules::RoleKeys) {
                    if (m_draft.roles.count(r)) roles.push_back(r);
                }
                if (m_draft.myRole.empty() || !m_draft.roles.count(m_draft.myRole)) m_draft.myRole = roles.empty() ? "" : roles.front();
                std::vector<std::string> roleLabels;
                for (const auto& r : roles) roleLabels.push_back(EventRules::RoleLabel(r));
                int myRole = IndexOf(roles, m_draft.myRole);
                FormLabel(T("You come as"), LabelWidth());
                if (Combo("##myrole", roleLabels, myRole, S(150))) m_draft.myRole = roles[static_cast<size_t>(myRole)];
                if (m_draft.myProfession.empty()) m_draft.myProfession = Profession();
                ImGui::SameLine();
                int profession = IndexOf(EventRules::Professions, EventRules::ProfessionOrEmpty(m_draft.myProfession));
                if (EventRules::ProfessionOrEmpty(m_draft.myProfession).empty()) m_draft.myProfession = EventRules::Professions[0];
                if (Combo("##profession", EventRules::Professions, profession, S(170))) {
                    m_draft.myProfession = EventRules::Professions[static_cast<size_t>(profession)];
                }
            }
        } else {
            FormLabel(T("Places"), LabelWidth());
            Input("##places", m_draft.maxText, T("empty is open, or 2 to 100"), S(160));
        }
        EndCard();

        BeginCard("notes", Palette::Gold);
        Kicker(T("Notes"));
        Memo("##notes", m_draft.description, ImGui::GetContentRegionAvail().x - S(36), S(110));
        Small(T("What to bring, where to meet (optional, 1000 characters)"), Palette::Muted);
        EndCard();

        std::string createLabel = editing ? T("Save changes") : guild && m_draft.weekly ? T("Create weekly event") : T("Create event");
        if (Button(createLabel, Kind::Primary, !m_creating, S(210))) {
            if (editing) SaveEdit();
            else CreateNow();
        }
        if (editing) {
            ImGui::SameLine();
            // removing an event is asked twice: the first press changes the button, the second does it
            if (Button(m_confirmDelete ? T("Really delete?") : T("Delete event"), Kind::Danger, !m_creating, S(160))) RemoveEvent();
            if (guild && !m_draft.editSeriesId.empty()) {
                ImGui::SameLine();
                if (Button(T("Stop the weekly event"), Kind::Secondary, !m_creating, S(210))) StopSeries();
            }
        }
        m_createStatus.Draw();
        ImGui::EndChild();
    }

    void App::CreateNow() {
        Moment now = Now();
        std::string problem = m_draft.Validate(now);
        if (!problem.empty()) {
            m_createStatus.Set(problem, Palette::Red);
            return;
        }
        m_creating = true;
        m_createStatus.Set(T("Creating..."), Palette::Muted);
        auto api = m_api;
        EventDraft draft = m_draft;
        std::string zone = TimeZones::LocalWindowsId();
        m_worker.Run<std::string>(
            [api, draft, now, zone]() {
                if (draft.IsGuild()) {
                    NewGuildEvent e = draft.ToGuild(now);
                    return draft.weekly ? api->CreateGuildSeries(e, WeeklySlot::Of(e.startAt, zone)) : api->CreateGuild(e);
                }
                return api->CreateOpen(draft.ToOpen(now));
            },
            [this](std::string) {
                m_creating = false;
                m_draft.title.clear();
                m_draft.whenText.clear();
                m_draft.description.clear();
                m_draft.mapText.clear();
                m_createStatus.Set(T("Event made."), Palette::Green);
                Refresh(true);
            },
            [this](const ApiError& e) {
                m_creating = false;
                m_createStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::SaveEdit() {
        Moment now = Now();
        std::string problem = m_draft.Validate(now);
        if (!problem.empty()) {
            m_createStatus.Set(problem, Palette::Red);
            return;
        }
        m_creating = true;
        m_createStatus.Set(T("Saving..."), Palette::Muted);
        auto api = m_api;
        EventDraft draft = m_draft;
        m_worker.Do(
            [api, draft, now]() {
                if (draft.IsGuild()) api->UpdateGuildEvent(draft, now);
                else api->UpdateOpenEvent(draft, now);
            },
            [this]() {
                m_creating = false;
                m_createStatus.Clear();
                Refresh(true);
                StopEditing();
            },
            [this](const ApiError& e) {
                m_creating = false;
                m_createStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::RemoveEvent() {
        if (!m_confirmDelete) {
            m_confirmDelete = true;
            m_createStatus.Set(T("Press again to delete the event for good. Cancel it instead to keep it on the list as cancelled."), Palette::Gold);
            return;
        }
        m_creating = true;
        m_createStatus.Set(T("Deleting..."), Palette::Muted);
        auto api = m_api;
        EventDraft draft = m_draft;
        m_worker.Do(
            [api, draft]() {
                if (draft.IsGuild()) api->DeleteGuildEvent(draft.editId, false);
                else api->DeleteOpenEvent(draft.editId);
            },
            [this]() {
                m_creating = false;
                m_createStatus.Clear();
                Refresh(true);
                StopEditing();
            },
            [this](const ApiError& e) {
                m_creating = false;
                m_confirmDelete = false;
                m_createStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::StopSeries() {
        m_creating = true;
        m_createStatus.Set(T("Stopping..."), Palette::Muted);
        auto api = m_api;
        std::string series = m_draft.editSeriesId;
        m_worker.Do([api, series]() { api->StopSeries(series); },
                    [this]() {
                        m_creating = false;
                        m_createStatus.Clear();
                        Refresh(true);
                        StopEditing();
                    },
                    [this](const ApiError& e) {
                        m_creating = false;
                        m_createStatus.Set(ErrorText::For(e), Palette::Red);
                        Failed(e);
                    });
    }
}
