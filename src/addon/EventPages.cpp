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

        /// a chip that goes on the same line while there is room before [limit], and on the next one when there is not
        void FlowChip(const std::string& text, const ImVec4& tone, float limit, bool& first) {
            float width = ImGui::CalcTextSize(text.c_str()).x + S(18);
            if (!first) {
                ImGui::SameLine(0, S(6));
                if (ImGui::GetCursorPosX() + width > limit) ImGui::NewLine();
            }
            first = false;
            Chip(text, tone);
        }

        std::string Going(const EventInfo& e) {
            std::string going = T("{0} going", {e.Places()});
            if (e.maybe > 0) going += T(", {0} maybe", {std::to_string(e.maybe)});
            if (e.waitlist > 0) going += " +" + std::to_string(e.waitlist);
            return going;
        }

        std::string When(const EventInfo& e, Moment now) {
            return TimeInput::Describe(e.startAt, now) + (e.durationMinutes > 0 ? "  " + EventRules::DurationLabel(e.durationMinutes) : "");
        }
    }

    // ---- the lists

    /// the filters of the website for the public events: what they are, in which language, for which region, with which tag
    void App::PublicPage() {
        std::vector<std::string> activities{T("Any activity")};
        for (const auto& a : EventRules::OpenActivities) activities.push_back(EventRules::ActivityLabel(a));
        std::vector<std::string> languages{T("Any language")};
        languages.insert(languages.end(), EventRules::Languages.begin() + 1, EventRules::Languages.end());
        std::vector<std::string> regions{T("All regions"), "EU", "NA"};
        std::vector<std::string> tags{T("Any tag")};
        for (const auto& t : EventRules::Tags) tags.push_back(EventRules::TagLabel(t));

        if (Combo("##activity", activities, m_publicActivity, S(180)))
            m_publicFilter.activity = m_publicActivity == 0 ? "" : EventRules::OpenActivities[static_cast<size_t>(m_publicActivity - 1)];
        ImGui::SameLine();
        if (Combo("##language", languages, m_publicLanguage, S(150)))
            m_publicFilter.language = m_publicLanguage == 0 ? "" : EventRules::Languages[static_cast<size_t>(m_publicLanguage)];
        ImGui::SameLine();
        if (Combo("##region", regions, m_publicRegion, S(140))) m_publicFilter.region = m_publicRegion == 1 ? "eu" : m_publicRegion == 2 ? "na" : "";
        ImGui::SameLine();
        if (Combo("##tag", tags, m_publicTag, S(170))) m_publicFilter.tag = m_publicTag == 0 ? "" : EventRules::Tags[static_cast<size_t>(m_publicTag - 1)];
        ImGui::SameLine();
        if (Button(T("Search"), Kind::Primary, !m_loading)) {
            m_publicLimit = 50;
            Refresh(true);
        }
        std::vector<EventInfo> shown;
        for (const auto& e : m_open) {
            if (e.status != "cancelled") shown.push_back(e);
        }
        EventList("public", shown, T("No public event matches. Make one in Create."), static_cast<int>(m_open.size()) >= m_publicLimit);
    }

    /// the guild events are few: they are filtered here, by what they are and by guild
    void App::GuildPage() {
        std::vector<std::string> activities{T("Any activity")};
        for (const auto& a : EventRules::GuildActivities) activities.push_back(EventRules::ActivityLabel(a));
        std::vector<std::string> guilds{T("All guilds")};
        if (m_me) {
            for (const auto& g : m_me->guilds) {
                if (std::find(guilds.begin(), guilds.end(), g.Label()) == guilds.end()) guilds.push_back(g.Label());
            }
        }
        Combo("##gactivity", activities, m_guildActivity, S(180));
        ImGui::SameLine();
        Combo("##gguild", guilds, m_guildPick, S(320));
        std::string activity = m_guildActivity == 0 ? "" : EventRules::GuildActivities[static_cast<size_t>(m_guildActivity - 1)];
        std::string guild = m_guildPick == 0 ? "" : guilds[static_cast<size_t>(m_guildPick)];
        std::vector<EventInfo> shown;
        for (const auto& e : m_guild) {
            if ((activity.empty() || e.activity == activity) && (guild.empty() || e.guildLabel == guild)) shown.push_back(e);
        }
        std::string empty = m_me && m_me->guilds.empty() ? T("You are not in a guild that uses Wondrous Compass.") : T("No guild events coming up.");
        EventList("guild", shown, empty, false);
    }

    void App::MinePage() {
        std::vector<EventInfo> mine;
        for (const auto& e : m_mine) {
            if (e.status != "cancelled") mine.push_back(e);
        }
        for (const auto& e : m_guild) {
            if (e.Joined() || e.mine) mine.push_back(e);
        }
        std::stable_sort(mine.begin(), mine.end(), [](const EventInfo& a, const EventInfo& b) { return a.startAt < b.startAt; });
        EventList("mine", mine, T("You have not signed up for anything."), false);
    }

    void App::EventList(const char* id, const std::vector<EventInfo>& events, const std::string& empty, bool canLoadMore) {
        ImGui::BeginChild(id, ImVec2(0, 0), false);
        if (events.empty()) {
            if (!m_loading) Wrapped(empty, Palette::Muted);
        } else {
            Moment now = Now();
            for (const auto& e : events) EventCard(e, now);
            if (canLoadMore && Button(T("Load more"), Kind::Secondary, !m_loading, S(220))) {
                m_publicLimit += 50;
                Refresh(true);
            }
        }
        ImGui::EndChild();
    }

    /// an event as a card of the list: the title, what it is, when, how many, the roles, the notes, and how the player stands; the buttons to
    /// sign up, say maybe or leave on the right. a click on the card opens the page of the event
    void App::EventCard(const EventInfo& e, Moment now) {
        bool over = e.Ended(now);
        std::vector<std::string> roles;
        for (const auto& r : e.roles) {
            if (!r.Full() || r.role == e.myRole) roles.push_back(r.role);
        }
        bool wantsRole = !over && !e.Joined() && std::any_of(e.roles.begin(), e.roles.end(), [](const RolePlace& r) { return !r.Full(); });
        bool busy = m_busy.count(e.id) > 0;

        BeginCard(e.id.c_str(), over ? Palette::Line : Palette::Activity(e.activity), true);
        ImVec2 top = ImGui::GetCursorPos();
        float width = ImGui::GetContentRegionAvail().x;
        float buttons = S(124);
        float rightX = top.x + width - buttons - S(18);
        float limit = rightX - (wantsRole ? S(160) : 0) - S(8);

        // the buttons first, in the corner, then what the event is beside them
        if (!over) {
            ImGui::SetCursorPos(ImVec2(rightX, top.y));
            if (e.Joined()) {
                if (Button(T("Leave"), Kind::Secondary, !busy, buttons)) Leave(e);
            } else {
                int& pick = m_rolePick[e.id];
                std::string role = wantsRole && !roles.empty() ? roles[static_cast<size_t>(std::min<int>(pick, static_cast<int>(roles.size()) - 1))] : "";
                if (Button(T("Sign up"), Kind::Primary, !busy, buttons)) Join(e, role, "going");
                ImGui::SetCursorPos(ImVec2(rightX, ImGui::GetCursorPosY()));
                if (Button(T("Maybe"), Kind::Secondary, !busy, buttons)) Join(e, role, "maybe");
                if (wantsRole && !roles.empty()) {
                    ImGui::SetCursorPos(ImVec2(rightX - S(152), top.y));
                    std::vector<std::string> labels;
                    for (const auto& r : roles) labels.push_back(EventRules::RoleLabel(r));
                    std::string comboId = "##role" + e.id;
                    Combo(comboId.c_str(), labels, pick, S(144));
                }
            }
        }

        ImGui::SetCursorPos(top);
        ImGui::PushTextWrapPos(limit);
        Title(Shorten(e.title, 60), over ? Palette::Muted : e.mine ? Palette::GoldSoft : Palette::Text);

        bool first = true;
        FlowChip(EventRules::ActivityLabel(e.activity), Palette::Activity(e.activity), limit, first);
        FlowChip(When(e, now), Palette::Gold, limit, first);
        FlowChip(Going(e), Palette::TextSoft, limit, first);
        if (over) FlowChip(T("Ended"), Palette::Red, limit, first);
        if (e.isGuild) FlowChip(e.guildLabel, Palette::Blue, limit, first);
        if (e.mapId) FlowChip(T("Map {0}", {std::to_string(*e.mapId)}), Palette::Muted, limit, first);
        if (!e.organizer.empty()) {
            std::string by = T("by {0}", {e.organizer});
            if (ImGui::GetCursorPosX() + ImGui::CalcTextSize(by.c_str()).x < limit) ImGui::SameLine(0, S(8));
            ImGui::AlignTextToFramePadding();
            Text(by, Palette::Muted);
        }

        // the roles: green while there is room, red when it is full. a guild event says only how many each role takes
        if (!e.roles.empty()) {
            bool firstRole = true;
            for (const auto& r : e.roles) {
                std::string text = EventRules::RoleLabel(r.role) + (e.isGuild ? " " + std::to_string(r.slots) : " " + std::to_string(r.taken) + "/" + std::to_string(r.slots));
                FlowChip(text, e.isGuild ? Palette::TextSoft : r.Full() ? Palette::Red : Palette::Green, limit, firstRole);
            }
        }
        if (!Trimmed(e.description).empty()) Small(Shorten(e.description, 140), Palette::Muted);
        if (e.Joined()) {
            std::string as = e.myRole.empty() ? "" : T(" as {0}", {EventRules::RoleLabel(e.myRole)});
            Small(T(over ? "You were {0}" : "You are {0}", {EventRules::StatusLabel(e.myStatus) + as}), Palette::Green);
        }
        ImGui::PopTextWrapPos();
        // the card is at least as high as its two buttons
        float bottom = top.y + S(84);
        if (ImGui::GetCursorPosY() < bottom) ImGui::Dummy(ImVec2(1, bottom - ImGui::GetCursorPosY()));
        if (EndCard()) OpenDetails(e);
    }

    void App::Join(const EventInfo& e, const std::string& role, const std::string& status) {
        std::string profession = Profession();
        auto api = m_api;
        std::string id = e.id;
        if (e.isGuild) {
            std::string account;
            if (m_me) {
                for (const auto& g : m_me->guilds) {
                    if (g.guildId == e.guildId) {
                        account = g.accountId;
                        break;
                    }
                }
            }
            Act(id, [api, id, status, role, profession, account]() { api->Rsvp(id, status, role, profession, account); });
            return;
        }
        // the account the player chose to sign up with, when they still have it; otherwise the server takes the first of the region
        std::string chosen;
        if (m_me && !m_settings.defaultAccountId.empty()) {
            for (const auto& a : m_me->accounts) {
                if (a.id == m_settings.defaultAccountId) chosen = a.id;
            }
        }
        Act(id, [api, id, status, chosen, role, profession]() { api->JoinOpen(id, status, chosen, role, profession); });
    }

    void App::Leave(const EventInfo& e) {
        auto api = m_api;
        std::string id = e.id;
        if (e.isGuild) {
            std::string account;
            if (m_me) {
                for (const auto& g : m_me->guilds) {
                    if (g.guildId == e.guildId) {
                        account = g.accountId;
                        break;
                    }
                }
            }
            Act(id, [api, id, account]() { api->Rsvp(id, "declined", "", "", account); });
            return;
        }
        Act(id, [api, id]() { api->LeaveOpen(id); });
    }

    /// does a call and, when it went through, reads the lists again
    void App::Act(const std::string& busyKey, std::function<void()> call) {
        m_busy.insert(busyKey);
        m_worker.Do(
            call,
            [this, busyKey]() {
                m_busy.erase(busyKey);
                Refresh(true);
            },
            [this, busyKey](const ApiError& err) {
                m_busy.erase(busyKey);
                Failed(err);
            });
    }

    // ---- the page of an event

    void App::OpenDetails(const EventInfo& e) {
        m_details = e;
        m_roster.clear();
        m_rosterStatus.Clear();
        m_flex = std::set<std::string>(e.myFlex.begin(), e.myFlex.end());
        m_flexStatus.Clear();
        m_reportStatus.Clear();
        m_reportReason = 0;
        m_reported = false;
        m_copied.clear();
        Show(Page::Details);
        // who signed up to a public event is for its organizer only: the server refuses anybody else, which is no error to show them
        m_rosterLoading = e.isGuild || e.mine;
        if (!m_rosterLoading) return;
        auto api = m_api;
        std::string id = e.id;
        bool guild = e.isGuild;
        m_worker.Run<std::vector<RosterEntry>>(
            [api, id, guild]() { return guild ? api->GuildRoster(id) : api->OpenRoster(id); },
            [this, id](std::vector<RosterEntry> rows) {
                if (!m_details || m_details->id != id) return;
                m_rosterLoading = false;
                m_roster = std::move(rows);
            },
            [this, id](const ApiError& err) {
                if (!m_details || m_details->id != id) return;
                m_rosterLoading = false;
                m_rosterStatus.Set(ErrorText::For(err), Palette::Red);
                if (err.IsInvalidToken()) Failed(err);
            });
    }

    void App::DetailsPage() {
        if (!m_details) {
            Show(m_back);
            return;
        }
        const EventInfo e = *m_details;
        Moment now = Now();
        bool over = e.Ended(now);

        if (Button(T("Back"), Kind::Quiet)) {
            Show(m_back);
            return;
        }
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        Title(Shorten(e.title, 48), Palette::GoldSoft);

        // what can be done with the event, in the corner
        float right = ImGui::GetWindowContentRegionMax().x;
        auto Corner = [&](const std::string& label, float least) {
            float width = ButtonWidth(label, least);
            right -= width;
            ImGui::SameLine(right);
            right -= S(8);
            return width;
        };
        if (!e.isGuild) {
            std::string label = m_copied == e.id ? T("Copied") : T("Copy link");
            float width = Corner(label, S(150));
            if (Button(label, Kind::Secondary, true, width)) {
                Copy(PublicEventLink(e.id));
                m_copied = e.id;
            }
        }
        if (e.CanChange() && !over) {
            float width = Corner(T("Edit"), S(100));
            if (Button(T("Edit"), Kind::Secondary, true, width)) {
                EditEvent(e);
                Show(Page::Create);
                return;
            }
        }
        if (e.mine && !over) {
            float width = Corner(T("Cancel event"), S(150));
            if (Button(T("Cancel event"), Kind::Danger, !m_busy.count(e.id), width)) {
                auto api = m_api;
                std::string id = e.id;
                bool guild = e.isGuild;
                Act(id, [api, id, guild]() {
                    if (guild) api->CancelGuild(id);
                    else api->CancelOpen(id);
                });
                Show(m_back);
                return;
            }
        }

        ImGui::BeginChild("details", ImVec2(0, 0), false);

        // what it is, and all that was written of it
        BeginCard("overview", Palette::Activity(e.activity));
        float limit = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
        bool first = true;
        FlowChip(EventRules::ActivityLabel(e.activity), Palette::Activity(e.activity), limit, first);
        FlowChip(When(e, now), Palette::Gold, limit, first);
        FlowChip(Going(e), Palette::TextSoft, limit, first);
        if (e.isGuild) FlowChip(e.guildLabel, Palette::Blue, limit, first);
        if (e.mapId) FlowChip(T("Map {0}", {std::to_string(*e.mapId)}), Palette::Muted, limit, first);
        if (over) FlowChip(T("Ended"), Palette::Red, limit, first);
        if (!e.organizer.empty()) Small(T("by {0}", {e.organizer}), Palette::Muted);
        if (!Trimmed(e.description).empty()) Wrapped(e.description, Palette::Text);
        EndCard();

        // the roles the player could also play, which the organizer sees when the roles are full
        if (e.isGuild && e.Joined() && !over && e.roles.size() >= 2) {
            BeginCard("flex", Palette::Gold);
            Kicker(T("Spare roles"));
            Small(T("Roles you could also play, so that the organizer can move you when one is full."), Palette::Muted);
            for (const auto& r : e.roles) {
                if (r.role == e.myRole) continue;
                bool on = m_flex.count(r.role) > 0;
                if (Check(EventRules::RoleLabel(r.role), on)) {
                    if (on) m_flex.insert(r.role);
                    else m_flex.erase(r.role);
                }
                ImGui::SameLine(0, S(18));
            }
            ImGui::NewLine();
            if (Button(T("Save"), Kind::Primary, !m_busy.count("flex"))) {
                m_busy.insert("flex");
                auto api = m_api;
                std::string id = e.id;
                std::vector<std::string> chosen(m_flex.begin(), m_flex.end());
                m_worker.Do([api, id, chosen]() { api->SetFlexRoles(id, chosen); },
                            [this, chosen]() {
                                m_busy.erase("flex");
                                if (m_details) m_details->myFlex = chosen;
                                m_flexStatus.Set(T("Saved."), Palette::Green);
                            },
                            [this](const ApiError& err) {
                                m_busy.erase("flex");
                                m_flexStatus.Set(ErrorText::For(err), Palette::Red);
                                Failed(err);
                            });
            }
            ImGui::SameLine();
            m_flexStatus.Draw();
            EndCard();
        }

        // somebody else's public event can be reported
        if (!e.isGuild && !e.mine) {
            BeginCard("report", Palette::Line);
            Kicker(T("Report this event"));
            std::vector<std::string> reasons{T("Spam"), T("Offensive"), T("Not Guild Wars 2"), T("Other")};
            static const char* keys[] = {"spam", "offensive", "not_gw2", "other"};
            Combo("##reason", reasons, m_reportReason, S(200));
            ImGui::SameLine();
            if (Button(T("Report this event"), Kind::Secondary, !m_reported && !m_busy.count("report"))) {
                m_busy.insert("report");
                auto api = m_api;
                std::string id = e.id, key = keys[m_reportReason];
                m_worker.Do([api, id, key]() { api->ReportOpenEvent(id, key); },
                            [this]() {
                                m_busy.erase("report");
                                m_reported = true;
                                m_reportStatus.Set(T("Thank you, a moderator will look at it."), Palette::Green);
                            },
                            [this](const ApiError& err) {
                                m_busy.erase("report");
                                m_reportStatus.Set(ErrorText::For(err), Palette::Red);
                            });
            }
            m_reportStatus.Draw();
            EndCard();
        }

        // who is in: everybody of the guild sees it, of a public event the organizer
        if (!e.isGuild && !e.mine) {
            // nothing: the list is not theirs to see
        } else if (m_rosterLoading) {
            Text(T("Loading..."), Palette::Muted);
        } else if (!m_rosterStatus.text.empty()) {
            m_rosterStatus.Draw();
        } else if (m_roster.empty()) {
            Text(T("Nobody yet."), Palette::Muted);
        } else {
            int n = 0;
            for (const auto& r : m_roster) {
                std::string id = "roster" + std::to_string(n++);
                BeginCard(id.c_str(), r.status == "going" ? Palette::Green : Palette::Muted);
                ImGui::AlignTextToFramePadding();
                Strong(r.name, Palette::Text);
                ImGui::SameLine(S(300));
                Chip(EventRules::StatusLabel(r.status), r.status == "going" ? Palette::Green : Palette::Muted);
                if (!r.role.empty()) {
                    ImGui::SameLine(0, S(6));
                    Chip(EventRules::RoleLabel(r.role), Palette::Gold);
                }
                if (!r.profession.empty()) {
                    ImGui::SameLine(0, S(6));
                    Chip(r.profession, Palette::Blue);
                }
                if (!r.flex.empty()) {
                    std::string also;
                    for (const auto& f : r.flex) also += (also.empty() ? "" : ", ") + EventRules::RoleLabel(f);
                    ImGui::SameLine(0, S(8));
                    Text(T("also {0}", {also}), Palette::Muted);
                }
                EndCard();
            }
        }
        ImGui::EndChild();
    }
}
