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

        void FlowChip(const std::string& text, const ImVec4& tone, float limit, bool& first) {
            float width = ImGui::CalcTextSize(text.c_str()).x + S(18);
            if (!first) {
                ImGui::SameLine(0, S(6));
                if (ImGui::GetCursorPosX() + width > limit) ImGui::NewLine();
            }
            first = false;
            Chip(text, tone);
        }

        /// a part of the page of a guild, when it says anything
        void Section(const char* id, const std::string& kicker, const std::string& text) {
            if (Trimmed(text).empty()) return;
            BeginCard(id, Palette::Line);
            Kicker(kicker);
            Wrapped(text, Palette::Text);
            EndCard();
        }
    }

    // ---- finding a guild

    /// the guilds that are recruiting: a list with the filters of the website, and the page of a guild with how to reach it
    void App::FindPage() {
        if (m_guildOpen) {
            GuildDetail();
            return;
        }
        std::vector<std::string> languages{T("Any language")};
        for (const auto& l : RecruitRules::Languages) languages.push_back(RecruitRules::LanguageLabel(l));
        std::vector<std::string> regions{T("Any region"), "EU", "NA"};
        std::vector<std::string> tags{T("Anything")};
        for (const auto& t : RecruitRules::Tags) tags.push_back(RecruitRules::TagLabel(t));

        Input("##search", m_findFilter.search, T("Name, tag or headline"), S(170));
        ImGui::SameLine();
        if (Combo("##flanguage", languages, m_findLanguage, S(130)))
            m_findFilter.language = m_findLanguage == 0 ? "" : RecruitRules::Languages[static_cast<size_t>(m_findLanguage - 1)];
        ImGui::SameLine();
        if (Combo("##fregion", regions, m_findRegion, S(120))) m_findFilter.region = m_findRegion == 1 ? "eu" : m_findRegion == 2 ? "na" : "";
        ImGui::SameLine();
        if (Combo("##ftag", tags, m_findTag, S(140))) m_findFilter.lookingFor = m_findTag == 0 ? "" : RecruitRules::Tags[static_cast<size_t>(m_findTag - 1)];
        ImGui::SameLine();
        Check(T("Recruiting now"), m_findFilter.openOnly);
        ImGui::SameLine();
        if (Button(T("Search"), Kind::Primary, !m_finding)) LoadGuilds();
        m_findStatus.Draw();

        ImGui::BeginChild("guilds", ImVec2(0, 0), false);
        for (const auto& c : m_found.rows) {
            BeginCard(c.guildId.c_str(), c.recruiting ? Palette::Green : Palette::Muted, true);
            ImVec2 top = ImGui::GetCursorPos();
            float width = ImGui::GetContentRegionAvail().x;
            float rightX = top.x + width - S(124) - S(18);
            ImGui::SetCursorPos(ImVec2(rightX, top.y));
            bool open = Button(T("Details"), Kind::Primary, true, S(124));
            if (!c.playTimes.empty()) {
                ImGui::SetCursorPos(ImVec2(rightX - S(120), ImGui::GetCursorPosY()));
                ImGui::PushTextWrapPos(rightX + S(124));
                Small(c.playTimes, Palette::Muted);
                ImGui::PopTextWrapPos();
            }
            ImGui::SetCursorPos(top);
            float limit = rightX - S(12);
            ImGui::PushTextWrapPos(limit);
            Title(Shorten(c.Title(), 60), Palette::Text);
            Text(c.headline, Palette::TextSoft);
            bool first = true;
            FlowChip(c.recruiting ? T("Recruiting") : T("Not recruiting"), c.recruiting ? Palette::Green : Palette::Muted, limit, first);
            FlowChip(RecruitRules::RegionLabel(c.region), Palette::Blue, limit, first);
            for (const auto& l : c.languages) FlowChip(RecruitRules::LanguageLabel(l), Palette::TextSoft, limit, first);
            for (const auto& t : c.lookingFor) FlowChip(RecruitRules::TagLabel(t), Palette::Gold, limit, first);
            ImGui::PopTextWrapPos();
            if (EndCard() || open) OpenGuild(c);
        }
        ImGui::EndChild();
    }

    void App::LoadGuilds() {
        if (m_finding) return;
        m_finding = true;
        m_findStatus.Set(T("Looking..."), Palette::Muted);
        auto api = m_api;
        RecruitFilter filter = m_findFilter;
        m_worker.Run<RecruitList>(
            [api, filter]() { return api->RecruitingGuilds(filter); },
            [this](RecruitList list) {
                m_finding = false;
                m_foundAt = Now();
                m_found = std::move(list);
                std::string said = m_found.total == 0 ? T("No guild matches.")
                                                      : (m_found.total == 1 ? T("{0} guild", {std::to_string(m_found.total)}) : T("{0} guilds", {std::to_string(m_found.total)}));
                if (m_found.total > static_cast<int>(m_found.rows.size())) said += T(", the first {0}", {std::to_string(m_found.rows.size())});
                m_findStatus.Set(said, Palette::Muted);
            },
            [this](const ApiError& e) {
                m_finding = false;
                m_foundAt = Now();
                m_findStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::OpenGuild(const RecruitCard& c) {
        m_guildOpen = true;
        m_guildPage.reset();
        m_guildReportStatus.Clear();
        m_guildReportReason = 0;
        m_copied.clear();
        m_guildStatus.Set(T("Loading {0}...", {c.Title()}), Palette::Muted);
        auto api = m_api;
        std::string id = c.guildId;
        m_worker.Run<std::optional<wc::RecruitPage>>(
            [api, id]() { return api->RecruitmentPage(id); },
            [this](std::optional<wc::RecruitPage> page) {
                m_guildPage = std::move(page);
                if (m_guildPage) m_guildStatus.Clear();
                else m_guildStatus.Set(T("This page is no longer shown."), Palette::Muted);
            },
            [this](const ApiError& e) {
                m_guildStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::GuildDetail() {
        if (Button(T("Back"), Kind::Quiet)) {
            m_guildOpen = false;
            return;
        }
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        m_guildStatus.Draw();
        if (!m_guildPage) return;
        const wc::RecruitPage& page = *m_guildPage;

        ImGui::BeginChild("guild", ImVec2(0, 0), false);
        BeginCard("head", Palette::Gold);
        float limit = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - S(36);
        Title(page.Title(), Palette::GoldSoft);
        Text(page.headline, Palette::Text);
        bool first = true;
        FlowChip(page.recruiting ? T("Recruiting") : T("Not recruiting"), page.recruiting ? Palette::Green : Palette::Muted, limit, first);
        FlowChip(RecruitRules::RegionLabel(page.region), Palette::Blue, limit, first);
        for (const auto& l : page.languages) FlowChip(RecruitRules::LanguageLabel(l), Palette::TextSoft, limit, first);
        for (const auto& t : page.lookingFor) FlowChip(RecruitRules::TagLabel(t), Palette::Gold, limit, first);
        EndCard();

        Section("about", T("About"), page.description);
        Section("requirements", T("Requirements"), page.requirements);
        Section("times", T("When they play"), page.playTimes);

        // how to reach them: the name to whisper in the game, and the Discord invite, each with a button that copies it
        BeginCard("reach", Palette::Line);
        Kicker(T("How to reach them"));
        float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - S(36) - S(120);
        auto Line = [&](const std::string& label, const std::string& value, const char* id) {
            ImGui::AlignTextToFramePadding();
            Text(label, Palette::Text);
            ImGui::SameLine(right);
            ImGui::PushID(id);
            if (Button(m_copied == value ? T("Copied") : T("Copy"), Kind::Secondary, true, S(120))) {
                Copy(value);
                m_copied = value;
            }
            ImGui::PopID();
        };
        if (!Trimmed(page.contact).empty()) Line(T("Whisper  {0}", {page.contact}), page.contact, "whisper");
        if (!Trimmed(page.discordInvite).empty()) Line(T("Discord  {0}", {page.discordInvite}), page.discordInvite, "discord");
        if (Trimmed(page.contact).empty() && Trimmed(page.discordInvite).empty()) Small(T("The guild gave no way to be reached."), Palette::Muted);
        EndCard();

        // what the player sees, and what the moderators are told, which is in English
        BeginCard("report", Palette::Line);
        std::vector<std::string> reasons{T("Spam"), T("Misleading"), T("Offensive"), T("Other")};
        static const char* keys[] = {"Spam", "Misleading", "Offensive", "Other"};
        Combo("##greason", reasons, m_guildReportReason, S(190));
        ImGui::SameLine();
        if (Button(T("Report page"), Kind::Secondary, !m_busy.count("greport"))) {
            m_busy.insert("greport");
            auto api = m_api;
            std::string id = page.guildId, key = keys[m_guildReportReason];
            m_worker.Do([api, id, key]() { api->ReportRecruitment(id, key); },
                        [this]() {
                            m_busy.erase("greport");
                            m_guildReportStatus.Set(T("Thank you, a moderator will look at it."), Palette::Green);
                        },
                        [this](const ApiError& e) {
                            m_busy.erase("greport");
                            m_guildReportStatus.Set(ErrorText::For(e), Palette::Red);
                        });
        }
        m_guildReportStatus.Draw();
        EndCard();
        ImGui::EndChild();
    }

    // ---- writing the page of a guild

    /// the pages of the guilds the player may write: the leader of a guild, and the recruiters the leader named. a page is made and saved as a
    /// draft, and shown to everybody when it is ready, as on the website
    void App::ManagePage() {
        if (m_editing) {
            Editor();
            return;
        }
        Kicker(T("Guilds you recruit for"));
        if (!m_myGuildsStatus.text.empty()) {
            ImGui::SameLine(0, S(16));
            m_myGuildsStatus.Draw();
        }
        ImGui::BeginChild("myguilds", ImVec2(0, 0), false);
        if (!m_myGuildsLoading && m_myGuilds.empty() && m_myGuildsAt != 0 && m_myGuildsStatus.text.empty()) {
            Wrapped(T("You do not manage the page of any guild. The leader of a guild can write its page, and can name recruiters who may write it too (in the app, under Guilds)."),
                    Palette::Muted);
        }
        for (const auto& g : m_myGuilds) {
            ImVec4 tone = g.hidden ? Palette::Red : g.published && !g.stale ? Palette::Green : g.hasPage ? Palette::Gold : Palette::Muted;
            BeginCard(g.guildId.c_str(), tone);
            ImVec2 top = ImGui::GetCursorPos();
            std::string editLabel = g.hasPage ? T("Edit page") : T("Write page");
            float editWidth = ButtonWidth(editLabel, S(150));
            float rightX = top.x + ImGui::GetContentRegionAvail().x - editWidth - S(18);
            ImGui::SetCursorPos(ImVec2(rightX, top.y + S(12)));
            bool edit = Button(editLabel, g.hasPage ? Kind::Secondary : Kind::Primary, true, editWidth);
            ImGui::SetCursorPos(top);
            Title(Shorten(g.Title(), 50), Palette::Text);
            Chip(g.Status(), tone);
            if (g.isLeader) {
                ImGui::SameLine(0, S(6));
                Chip(T("Leader"), Palette::Gold);
            }
            EndCard();
            if (edit) OpenEditor(g);
        }
        ImGui::EndChild();
    }

    void App::LoadMyGuilds() {
        if (m_myGuildsLoading || !m_api->Linked()) return;
        m_myGuildsLoading = true;
        m_myGuildsStatus.Set(T("Loading..."), Palette::Muted);
        auto api = m_api;
        m_worker.Run<std::vector<MyRecruitGuild>>(
            [api]() { return api->MyRecruitmentGuilds(); },
            [this](std::vector<MyRecruitGuild> guilds) {
                m_myGuildsLoading = false;
                m_myGuildsAt = Now();
                m_myGuilds = std::move(guilds);
                m_myGuildsStatus.Clear();
            },
            [this](const ApiError& e) {
                m_myGuildsLoading = false;
                m_myGuildsAt = Now();
                m_myGuildsStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::OpenEditor(const MyRecruitGuild& g) {
        m_editing = g;
        m_recruitPage.reset();
        m_pageDraft = RecruitDraft();
        m_confirmPageDelete = false;
        m_recruiters.clear();
        m_members.clear();
        m_pageStatus.Clear();
        if (!g.hasPage) return;
        m_pageLoading = true;
        auto api = m_api;
        std::string id = g.guildId;
        m_worker.Run<std::optional<wc::RecruitPage>>(
            [api, id]() { return api->RecruitmentPage(id); },
            [this, id](std::optional<wc::RecruitPage> page) {
                if (!m_editing || m_editing->guildId != id) return;
                m_pageLoading = false;
                m_recruitPage = std::move(page);
                m_pageDraft = RecruitDraft::From(m_recruitPage);
                LoadRecruiters();
            },
            [this, id](const ApiError& e) {
                if (!m_editing || m_editing->guildId != id) return;
                m_pageLoading = false;
                m_pageStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::LoadRecruiters() {
        if (!m_editing || !m_editing->isLeader || !m_recruitPage) return;
        auto api = m_api;
        std::string id = m_editing->guildId;
        struct People {
            std::vector<Recruiter> allowed, members;
        };
        m_worker.Run<People>(
            [api, id]() { return People{api->GuildRecruiters(id), api->GuildMembers(id)}; },
            [this, id](People p) {
                if (!m_editing || m_editing->guildId != id) return;
                m_recruiters = std::move(p.allowed);
                m_members = std::move(p.members);
                m_allowPick = 0;
            },
            [this](const ApiError& e) { m_pageStatus.Set(ErrorText::For(e), Palette::Red); });
    }

    void App::Editor() {
        MyRecruitGuild& g = *m_editing;
        if (Button(T("Back"), Kind::Quiet)) {
            m_editing.reset();
            LoadMyGuilds();
            return;
        }
        if (m_pageLoading) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            Text(T("Loading {0}...", {g.Title()}), Palette::Muted);
            return;
        }
        bool published = m_recruitPage && m_recruitPage->published;
        ImGui::BeginChild("editor", ImVec2(0, 0), false);
        float field = std::min(S(560), ImGui::GetContentRegionAvail().x - LabelWidth() - S(60));

        ImGui::AlignTextToFramePadding();
        Title(Shorten(g.Title(), 40), Palette::GoldSoft);
        ImGui::SameLine(0, S(14));
        ImVec4 tone = m_recruitPage && m_recruitPage->hidden ? Palette::Red : published ? Palette::Green : Palette::Gold;
        Chip(!m_recruitPage ? T("No page yet") : m_recruitPage->hidden ? T("Hidden by a moderator") : published ? T("Shown to everybody") : T("Draft"), tone);
        if (m_recruitPage && m_recruitPage->hidden && !m_recruitPage->hiddenReason.empty()) Small(T("Reason: {0}", {m_recruitPage->hiddenReason}), Palette::Red);

        BeginCard("about", Palette::Gold);
        Kicker(T("About"));
        FormLabel(T("Headline"), LabelWidth());
        Input("##headline", m_pageDraft.headline, T("One line that makes players read on (80)"), field);
        FormLabel(T("Description"), LabelWidth());
        Memo("##description", m_pageDraft.description, field, S(126));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + LabelWidth());
        Small(T("Who you are and what you do (20 to 2000 characters)"), Palette::Muted);
        FormLabel(T("Requirements"), LabelWidth());
        Input("##requirements", m_pageDraft.requirements, T("Level, mastery, experience... (500)"), field);
        FormLabel(T("When you play"), LabelWidth());
        Input("##playtimes", m_pageDraft.playTimes, T("Tue and Thu 20:00 server time (120)"), field);
        EndCard();

        BeginCard("reach", Palette::Gold);
        Kicker(T("How to reach them"));
        FormLabel(T("Whisper"), LabelWidth());
        Input("##contact", m_pageDraft.contact, T("Name.1234 in the game (120)"), S(320));
        FormLabel(T("Discord invite"), LabelWidth());
        Input("##invite", m_pageDraft.discordInvite, "https://discord.gg/...", S(420));
        EndCard();

        BeginCard("looking", Palette::Gold);
        Kicker(T("Looking for"));
        std::vector<std::string> regions;
        for (const auto& r : RecruitRules::Regions) regions.push_back(RecruitRules::RegionLabel(r));
        int region = static_cast<int>(std::find(RecruitRules::Regions.begin(), RecruitRules::Regions.end(), m_pageDraft.region) - RecruitRules::Regions.begin());
        FormLabel(T("Region"), LabelWidth());
        if (Combo("##pregion", regions, region, S(150))) m_pageDraft.region = RecruitRules::Regions[static_cast<size_t>(region)];
        ImGui::SameLine(0, S(26));
        Check(T("Recruiting now"), m_pageDraft.recruiting);

        // the language is one, and what the guild looks for is four at the most: with the language they are the five tags of the post
        std::vector<std::string> languages{T("Not said")};
        for (const auto& l : RecruitRules::Languages) languages.push_back(RecruitRules::LanguageLabel(l));
        int language = 0;
        if (!m_pageDraft.languages.empty()) {
            auto it = std::find(RecruitRules::Languages.begin(), RecruitRules::Languages.end(), *m_pageDraft.languages.begin());
            if (it != RecruitRules::Languages.end()) language = static_cast<int>(it - RecruitRules::Languages.begin()) + 1;
        }
        FormLabel(T("Language"), LabelWidth());
        if (Combo("##planguage", languages, language, S(200))) m_pageDraft.SetLanguage(language == 0 ? "" : RecruitRules::Languages[static_cast<size_t>(language - 1)]);

        FormLabel(T("Looking for"), LabelWidth());
        bool full = static_cast<int>(m_pageDraft.lookingFor.size()) >= RecruitRules::LookingForMax;
        Small(T("{0} of {1} picked (the language is the fifth tag of the post in the forum)",
                {std::to_string(m_pageDraft.lookingFor.size()), std::to_string(RecruitRules::LookingForMax)}),
              full ? Palette::Gold : Palette::Muted);
        float x = LabelWidth();
        float limit = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - S(36);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
        bool firstBox = true;
        float startX = ImGui::GetCursorPosX();
        for (const auto& key : RecruitRules::Tags) {
            std::string label = RecruitRules::TagLabel(key);
            float width = ImGui::CalcTextSize(label.c_str()).x + S(50);
            if (!firstBox) {
                ImGui::SameLine(0, S(14));
                if (ImGui::GetCursorPosX() + width > limit) {
                    ImGui::NewLine();
                    ImGui::SetCursorPosX(startX);
                }
            }
            firstBox = false;
            bool on = m_pageDraft.lookingFor.count(key) > 0;
            if (Check(label, on)) m_pageDraft.ToggleLookingFor(key, on);
        }
        EndCard();

        if (Button(T("Save"), Kind::Secondary, !m_pageBusy, S(150))) SavePage(false);
        ImGui::SameLine();
        if (Button(published ? T("Save and keep shown") : T("Save and show"), Kind::Primary, !m_pageBusy, S(230))) SavePage(true);
        if (published) {
            ImGui::SameLine();
            if (Button(T("Take back to draft"), Kind::Secondary, !m_pageBusy, S(200))) Unpublish();
        }
        m_pageStatus.Draw();

        // the leader says who else may write the page, and may take the page away
        if (g.isLeader && m_recruitPage) {
            BeginCard("recruiters", Palette::Gold);
            Kicker(T("Who may write this page"));
            Small(T("The leader always may. A member you allow can write, show and take back the page, but cannot name others or delete it."), Palette::Muted);
            float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - S(36) - S(140);
            for (const auto& r : m_recruiters) {
                ImGui::PushID(r.userId.c_str());
                ImGui::AlignTextToFramePadding();
                Strong(r.accountName, Palette::Text);
                ImGui::SameLine(right);
                if (Button(T("Take back"), Kind::Secondary, !m_pageBusy, S(140))) {
                    m_pageBusy = true;
                    auto api = m_api;
                    std::string guild = g.guildId, user = r.userId;
                    m_worker.Do([api, guild, user]() { api->RevokeRecruiter(guild, user); },
                                [this]() {
                                    m_pageBusy = false;
                                    LoadRecruiters();
                                },
                                [this](const ApiError& e) {
                                    m_pageBusy = false;
                                    m_pageStatus.Set(ErrorText::For(e), Palette::Red);
                                    Failed(e);
                                });
                }
                ImGui::PopID();
            }
            if (m_recruiters.empty()) Small(T("Nobody else yet."), Palette::Muted);

            // the members who are not the leader and are not allowed already, one for each person
            std::vector<Recruiter> candidates;
            for (const auto& m : m_members) {
                if (m.role == "leader") continue;
                if (std::any_of(m_recruiters.begin(), m_recruiters.end(), [&](const Recruiter& r) { return r.userId == m.userId; })) continue;
                if (std::any_of(candidates.begin(), candidates.end(), [&](const Recruiter& c) { return c.userId == m.userId; })) continue;
                candidates.push_back(m);
            }
            if (candidates.empty()) {
                Small(T("There is no member left to allow."), Palette::Muted);
            } else {
                std::vector<std::string> names;
                for (const auto& c : candidates) names.push_back(c.accountName);
                Combo("##allow", names, m_allowPick, S(300));
                ImGui::SameLine();
                if (Button(T("Allow"), Kind::Primary, !m_pageBusy, S(140))) {
                    m_pageBusy = true;
                    auto api = m_api;
                    std::string guild = g.guildId, user = candidates[static_cast<size_t>(std::min<int>(m_allowPick, static_cast<int>(candidates.size()) - 1))].userId;
                    m_worker.Do([api, guild, user]() { api->GrantRecruiter(guild, user); },
                                [this]() {
                                    m_pageBusy = false;
                                    LoadRecruiters();
                                },
                                [this](const ApiError& e) {
                                    m_pageBusy = false;
                                    m_pageStatus.Set(ErrorText::For(e), Palette::Red);
                                    Failed(e);
                                });
                }
            }
            EndCard();
            if (Button(m_confirmPageDelete ? T("Really delete the page?") : T("Delete the page"), Kind::Danger, !m_pageBusy, S(250))) DeletePage();
        }
        ImGui::EndChild();
    }

    void App::SavePage(bool show) {
        std::string problem = m_pageDraft.Validate(show);
        if (!problem.empty()) {
            m_pageStatus.Set(problem, Palette::Red);
            return;
        }
        m_pageBusy = true;
        m_pageStatus.Set(T("Saving..."), Palette::Muted);
        auto api = m_api;
        std::string id = m_editing->guildId;
        RecruitDraft draft = m_pageDraft;
        m_worker.Run<std::optional<wc::RecruitPage>>(
            [api, id, draft, show]() {
                auto page = api->SaveRecruitment(id, draft);
                if (show) page = api->SetRecruitmentPublished(id, true);
                return page;
            },
            [this, show, id](std::optional<wc::RecruitPage> page) {
                m_pageBusy = false;
                if (page) m_recruitPage = std::move(page);
                if (m_editing && m_editing->guildId == id) {
                    m_editing->hasPage = true;
                    m_editing->published = m_recruitPage && m_recruitPage->published;
                }
                m_foundAt = 0;
                m_pageStatus.Set(show ? T("Saved, and shown to everybody.") : T("Saved."), Palette::Green);
                LoadRecruiters();
            },
            [this](const ApiError& e) {
                m_pageBusy = false;
                m_pageStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::Unpublish() {
        m_pageBusy = true;
        m_pageStatus.Set(T("Taking it back..."), Palette::Muted);
        auto api = m_api;
        std::string id = m_editing->guildId;
        m_worker.Run<std::optional<wc::RecruitPage>>(
            [api, id]() { return api->SetRecruitmentPublished(id, false); },
            [this](std::optional<wc::RecruitPage> page) {
                m_pageBusy = false;
                if (page) m_recruitPage = std::move(page);
                if (m_editing) m_editing->published = false;
                m_foundAt = 0;
                m_pageStatus.Set(T("The page is a draft again and is no longer in the list."), Palette::Green);
            },
            [this](const ApiError& e) {
                m_pageBusy = false;
                m_pageStatus.Set(ErrorText::For(e), Palette::Red);
                Failed(e);
            });
    }

    void App::DeletePage() {
        if (!m_confirmPageDelete) {
            m_confirmPageDelete = true;
            m_pageStatus.Set(T("Press again to delete the page of the guild for good, with its post in the forum."), Palette::Gold);
            return;
        }
        m_pageBusy = true;
        auto api = m_api;
        std::string id = m_editing->guildId;
        m_worker.Do([api, id]() { api->DeleteRecruitment(id); },
                    [this]() {
                        m_pageBusy = false;
                        m_foundAt = 0;
                        m_editing.reset();
                        LoadMyGuilds();
                    },
                    [this](const ApiError& e) {
                        m_pageBusy = false;
                        m_confirmPageDelete = false;
                        m_pageStatus.Set(ErrorText::For(e), Palette::Red);
                        Failed(e);
                    });
    }
}
