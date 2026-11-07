#include "Api.h"

#include "Rules.h"

namespace wc {

    namespace {
        /// a filter or a field that is empty is sent as null, so the server does not filter by it
        Json Empty(const std::string& s) {
            std::string t = Trimmed(s);
            return t.empty() ? Json(nullptr) : Json(t);
        }

        Json OrNull(const std::string& s) { return s.empty() ? Json(nullptr) : Json(s); }
        Json OrNull(const std::optional<int>& n) { return n ? Json(*n) : Json(nullptr); }

        Json Tags(const std::vector<std::string>& tags) {
            Json a = Json::array();
            for (const auto& t : tags) a.push_back(t);
            return a;
        }

        std::string IdOf(const Json& j) {
            if (j.is_string()) return j.get<std::string>();
            if (j.is_null()) return "";
            return j.dump();
        }

        template <typename T, typename F>
        std::vector<T> Rows(const Json& j, F parse) {
            std::vector<T> list;
            if (!j.is_array()) return list;
            for (const auto& r : j) list.push_back(parse(r));
            return list;
        }
    }

    CodexApi::CodexApi(std::shared_ptr<IHttp> http, std::string publishableKey, std::string token, std::string baseUrl)
        : m_http(std::move(http)), m_key(std::move(publishableKey)), m_base(std::move(baseUrl)), m_token(std::move(token)) {
        while (!m_base.empty() && m_base.back() == '/') m_base.pop_back();
    }

    std::string CodexApi::Token() const {
        std::lock_guard<std::mutex> lock(m_lock);
        return m_token;
    }

    void CodexApi::SetToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(m_lock);
        m_token = token;
    }

    bool CodexApi::Linked() const { return !Token().empty(); }

    LinkResult CodexApi::Link(const std::string& code, const std::string& label) {
        Json j = Rpc("module_link_exchange", Json{{"p_code", code}, {"p_label", label}});
        LinkResult r;
        if (auto error = Str(j, "error")) {
            r.error = *error;
            return r;
        }
        r.token = StrOr(j, "token");
        r.name = StrOr(j, "name");
        if (!r.token.empty()) SetToken(r.token);
        return r;
    }

    MeInfo CodexApi::Me() { return MeInfo::Parse(Call("module_me", Json::object())); }

    std::vector<EventInfo> CodexApi::OpenEvents(const EventFilter& f, int limit, int offset) {
        Json j = Call("module_open_events", Json{{"p_activity", Empty(f.activity)}, {"p_language", Empty(f.language)}, {"p_limit", limit}, {"p_offset", offset},
                                                 {"p_region", Empty(f.region)}, {"p_tag", Empty(f.tag)}});
        return Rows<EventInfo>(j, EventInfo::ParseOpen);
    }

    std::vector<EventInfo> CodexApi::MyOpenEvents() { return Rows<EventInfo>(Call("module_my_open_events", Json::object()), EventInfo::ParseOpen); }

    std::vector<EventInfo> CodexApi::GuildEvents() { return Rows<EventInfo>(Call("module_guild_events", Json::object()), EventInfo::ParseGuild); }

    std::vector<RosterEntry> CodexApi::OpenRoster(const std::string& eventId) {
        return RosterEntry::ParseAll(Call("module_open_event_roster", Json{{"p_event", eventId}}));
    }

    std::vector<RosterEntry> CodexApi::GuildRoster(const std::string& eventId) {
        return RosterEntry::ParseAll(Call("module_event_roster", Json{{"p_event", eventId}}));
    }

    void CodexApi::JoinOpen(const std::string& eventId, const std::string& status, const std::string& accountId, const std::string& role, const std::string& profession) {
        Call("module_join_open_event", Json{{"p_event", eventId}, {"p_status", status}, {"p_account", OrNull(accountId)}, {"p_role", OrNull(role)},
                                            {"p_profession", OrNull(profession)}});
    }

    void CodexApi::LeaveOpen(const std::string& eventId) { Call("module_leave_open_event", Json{{"p_event", eventId}}); }

    void CodexApi::Rsvp(const std::string& eventId, const std::string& status, const std::string& role, const std::string& profession, const std::string& accountId) {
        Call("module_rsvp", Json{{"p_event", eventId}, {"p_status", status}, {"p_role", OrNull(role)}, {"p_profession", OrNull(profession)},
                                 {"p_account", OrNull(accountId)}});
    }

    void CodexApi::SetFlexRoles(const std::string& eventId, const std::vector<std::string>& roles) {
        Call("module_set_flex_roles", Json{{"p_event", eventId}, {"p_roles", Tags(roles)}});
    }

    std::string CodexApi::CreateOpen(const NewOpenEvent& e) {
        Json j = Call("module_create_open_event",
                      Json{{"p_title", e.title}, {"p_start_at", Clock::ToIsoUtc(e.startAt)}, {"p_duration_minutes", e.durationMinutes},
                           {"p_description", e.description}, {"p_activity", e.activity}, {"p_language", e.language},
                           {"p_max_participants", OrNull(e.maxParticipants)}, {"p_account", OrNull(e.accountId)}, {"p_region", e.region},
                           {"p_tags", Tags(e.tags)}, {"p_map_id", OrNull(e.mapId)}, {"p_roles", RolesJson(e.roles)}, {"p_my_role", OrNull(e.myRole)},
                           {"p_my_profession", OrNull(e.myProfession)}});
        return IdOf(j);
    }

    std::string CodexApi::CreateGuild(const NewGuildEvent& e) {
        Json j = Call("module_create_event",
                      Json{{"p_guild", e.guildId}, {"p_title", e.title}, {"p_start_at", Clock::ToIsoUtc(e.startAt)}, {"p_duration_minutes", e.durationMinutes},
                           {"p_description", e.description}, {"p_activity", e.activity}, {"p_max_participants", OrNull(e.maxParticipants)},
                           {"p_roles", RolesJson(e.roles)}, {"p_account", OrNull(e.accountId)}});
        return IdOf(j);
    }

    std::string CodexApi::CreateGuildSeries(const NewGuildEvent& e, const WeeklySlot& slot) {
        Json j = Call("module_create_event_series",
                      Json{{"p_guild", e.guildId}, {"p_title", e.title}, {"p_weekday", slot.weekday}, {"p_time_of_day", slot.timeOfDay},
                           {"p_timezone", slot.zone}, {"p_duration_minutes", e.durationMinutes}, {"p_description", e.description},
                           {"p_activity", e.activity}, {"p_max_participants", OrNull(e.maxParticipants)}, {"p_roles", RolesJson(e.roles)},
                           {"p_account", OrNull(e.accountId)}});
        return IdOf(j);
    }

    int CodexApi::UpdateGuildEvent(const EventDraft& d, Moment now) {
        NewGuildEvent g = d.ToGuild(now);
        Json j = Call("module_update_event",
                      Json{{"p_event", d.editId}, {"p_version", d.editVersion}, {"p_title", g.title}, {"p_start_at", Clock::ToIsoUtc(g.startAt)},
                           {"p_duration_minutes", g.durationMinutes}, {"p_description", g.description}, {"p_activity", g.activity},
                           {"p_max_participants", OrNull(g.maxParticipants)}, {"p_roles", RolesJson(g.roles)}});
        return j.is_number_integer() ? j.get<int>() : d.editVersion + 1;
    }

    void CodexApi::UpdateOpenEvent(const EventDraft& d, Moment now) {
        NewOpenEvent o = d.ToOpen(now);
        Call("module_update_open_event",
             Json{{"p_event", d.editId}, {"p_title", o.title}, {"p_start_at", Clock::ToIsoUtc(o.startAt)}, {"p_duration_minutes", o.durationMinutes},
                  {"p_description", o.description}, {"p_activity", o.activity}, {"p_language", o.language},
                  {"p_max_participants", OrNull(o.maxParticipants)}, {"p_tags", Tags(o.tags)}, {"p_map_id", OrNull(o.mapId)},
                  {"p_roles", RolesJson(o.roles)}});
    }

    void CodexApi::DeleteGuildEvent(const std::string& eventId, bool wholeSeries) {
        Call("module_delete_event", Json{{"p_event", eventId}, {"p_whole_series", wholeSeries}});
    }

    void CodexApi::StopSeries(const std::string& seriesId) { Call("module_stop_event_series", Json{{"p_series", seriesId}}); }

    void CodexApi::DeleteOpenEvent(const std::string& eventId) { Call("module_delete_open_event", Json{{"p_event", eventId}}); }

    void CodexApi::ReportOpenEvent(const std::string& eventId, const std::string& reason) {
        Call("module_report_open_event", Json{{"p_event", eventId}, {"p_reason", reason}, {"p_note", ""}});
    }

    void CodexApi::CancelOpen(const std::string& eventId) { Call("module_cancel_open_event", Json{{"p_event", eventId}}); }

    void CodexApi::CancelGuild(const std::string& eventId) { Call("module_cancel_event", Json{{"p_event", eventId}}); }

    RecruitList CodexApi::RecruitingGuilds(const RecruitFilter& f, int limit, int offset) {
        Json j = Rpc("recruiting_guilds", Json{{"p_language", Empty(f.language)}, {"p_region", Empty(f.region)}, {"p_looking_for", Empty(f.lookingFor)},
                                              {"p_search", Empty(f.search)}, {"p_open_only", f.openOnly}, {"p_limit", limit}, {"p_offset", offset}});
        return RecruitList::Parse(j);
    }

    std::optional<RecruitPage> CodexApi::RecruitmentPage(const std::string& guildId) {
        return RecruitPage::Parse(Call("module_recruitment_page", Json{{"p_guild", guildId}}));
    }

    std::vector<MyRecruitGuild> CodexApi::MyRecruitmentGuilds() { return MyRecruitGuild::ParseAll(Call("module_my_recruitment_guilds", Json::object())); }

    std::optional<RecruitPage> CodexApi::SaveRecruitment(const std::string& guildId, const RecruitDraft& draft) {
        return RecruitPage::Parse(Call("module_save_recruitment", Json{{"p_guild", guildId}, {"p_data", draft.ToJson()}}));
    }

    std::optional<RecruitPage> CodexApi::SetRecruitmentPublished(const std::string& guildId, bool published) {
        return RecruitPage::Parse(Call("module_set_recruitment_published", Json{{"p_guild", guildId}, {"p_published", published}}));
    }

    void CodexApi::ReportRecruitment(const std::string& guildId, const std::string& reason) {
        Call("module_report_recruitment", Json{{"p_guild", guildId}, {"p_reason", reason}});
    }

    std::vector<Recruiter> CodexApi::GuildRecruiters(const std::string& guildId) {
        return Recruiter::ParseAll(Call("module_guild_recruiters", Json{{"p_guild", guildId}}));
    }

    std::vector<Recruiter> CodexApi::GuildMembers(const std::string& guildId) {
        return Recruiter::ParseAll(Call("module_guild_roster", Json{{"p_guild", guildId}}));
    }

    void CodexApi::GrantRecruiter(const std::string& guildId, const std::string& userId) {
        Call("module_grant_recruiter", Json{{"p_guild", guildId}, {"p_user", userId}});
    }

    void CodexApi::RevokeRecruiter(const std::string& guildId, const std::string& userId) {
        Call("module_revoke_recruiter", Json{{"p_guild", guildId}, {"p_user", userId}});
    }

    void CodexApi::DeleteRecruitment(const std::string& guildId) { Call("module_delete_recruitment", Json{{"p_guild", guildId}}); }

    Json CodexApi::RolesJson(const std::map<std::string, int>& roles) {
        Json a = Json::array();
        // the roles of the form first, in their order, then any other the event had
        for (const auto& key : EventRules::RoleKeys) {
            auto it = roles.find(key);
            if (it != roles.end() && it->second > 0) a.push_back(Json{{"role", key}, {"slots", it->second}});
        }
        for (const auto& kv : roles) {
            bool known = false;
            for (const auto& key : EventRules::RoleKeys) known = known || key == kv.first;
            if (!known && kv.second > 0) a.push_back(Json{{"role", kv.first}, {"slots", kv.second}});
        }
        return a;
    }

    Json CodexApi::Call(const std::string& fn, Json args) {
        std::string token = Token();
        if (token.empty()) throw ApiError("invalid_token", "not linked", 401);
        if (!args.is_object()) args = Json::object();
        args["p_token"] = token;
        return Rpc(fn, args);
    }

    Json CodexApi::Rpc(const std::string& fn, const Json& body) {
        std::map<std::string, std::string> headers = {{"apikey", m_key}, {"Content-Type", "application/json"}, {"Accept", "application/json"}};
        HttpResponse res = m_http->Post(m_base + "/rest/v1/rpc/" + fn, headers, body.dump(-1, ' ', false, Json::error_handler_t::replace));
        if (res.status == 0) throw ApiError("network", res.error.empty() ? "the server was not reached" : res.error, 0);
        if (res.status < 200 || res.status >= 300) {
            std::string code, message;
            Json j = Json::parse(res.body, nullptr, false);
            if (!j.is_discarded() && j.is_object()) {
                code = StrOr(j, "hint");
                if (code.empty()) code = StrOr(j, "code");
                message = StrOr(j, "message");
            }
            throw ApiError(code.empty() ? "http_" + std::to_string(res.status) : code, message.empty() ? "http " + std::to_string(res.status) : message, res.status);
        }
        std::string text = Trimmed(res.body);
        if (text.empty()) return Json(nullptr);
        Json j = Json::parse(text, nullptr, false);
        if (j.is_discarded()) throw ApiError("bad_answer", "the server answered with something that is not json", res.status);
        return j;
    }
}
