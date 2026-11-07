#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Drafts.h"
#include "Models.h"

namespace wc {

    /// what a request came back with: the status and the text, or no status when the server was not reached
    struct HttpResponse {
        int status = 0;
        std::string body;
        std::string error;  // why it was not reached, when status is 0
    };

    /// sends a POST of json and waits for the answer. the addon does it with WinHTTP, the tests with a fake
    class IHttp {
    public:
        virtual ~IHttp() = default;
        virtual HttpResponse Post(const std::string& url, const std::map<std::string, std::string>& headers, const std::string& body) = 0;
    };

    /// the server refused a call, or was not reached. [code] is the hint the database gave (invalid_token, role_full, ...), "network" when the
    /// server was not reached and "bad_answer" when what came back was not json
    class ApiError : public std::runtime_error {
    public:
        ApiError(std::string code, const std::string& message, int status) : std::runtime_error(message), code(std::move(code)), status(status) {}

        std::string code;
        int status;

        /// the token was cut or never was one: the addon has to be linked again
        bool IsInvalidToken() const { return code == "invalid_token"; }
        bool IsNetwork() const { return code == "network"; }
    };

    struct LinkResult {
        std::string token, name, error;
        bool Ok() const { return error.empty() && !token.empty(); }
    };

    /// the module functions of the backend, called as PostgREST rpc. every call but the link and the list of recruiting guilds takes the token
    /// of the addon. each call waits for the server: they are made on the worker thread
    class CodexApi {
    public:
        static constexpr const char* DefaultBaseUrl = "https://zmujtvcuzujcgaholjkt.supabase.co";

        /// the publishable key of the backend: it is public by design, the database decides what it may do
        static constexpr const char* PublishableKey = "sb_publishable_22xN3RxpaF7iEuS8oJkRZA_iQcxefo4";

        CodexApi(std::shared_ptr<IHttp> http, std::string publishableKey = PublishableKey, std::string token = "", std::string baseUrl = DefaultBaseUrl);

        std::string Token() const;
        void SetToken(const std::string& token);
        bool Linked() const;

        // ---- linking

        /// swaps the code from the app for a token. the result has an error for a wrong or an old code
        LinkResult Link(const std::string& code, const std::string& label);

        // ---- reading

        MeInfo Me();
        std::vector<EventInfo> OpenEvents(const EventFilter& filter, int limit, int offset);
        std::vector<EventInfo> MyOpenEvents();
        std::vector<EventInfo> GuildEvents();
        std::vector<RosterEntry> OpenRoster(const std::string& eventId);
        std::vector<RosterEntry> GuildRoster(const std::string& eventId);

        // ---- signing up

        void JoinOpen(const std::string& eventId, const std::string& status, const std::string& accountId, const std::string& role, const std::string& profession);
        void LeaveOpen(const std::string& eventId);
        void Rsvp(const std::string& eventId, const std::string& status, const std::string& role, const std::string& profession, const std::string& accountId);

        /// the roles the player could also play, besides the one they signed up in. an empty list clears them
        void SetFlexRoles(const std::string& eventId, const std::vector<std::string>& roles);

        // ---- making, changing and removing

        std::string CreateOpen(const NewOpenEvent& e);
        std::string CreateGuild(const NewGuildEvent& e);

        /// a guild event that comes back every week at the same time, in the zone of the player
        std::string CreateGuildSeries(const NewGuildEvent& e, const WeeklySlot& slot);

        int UpdateGuildEvent(const EventDraft& d, Moment now);
        void UpdateOpenEvent(const EventDraft& d, Moment now);
        void DeleteGuildEvent(const std::string& eventId, bool wholeSeries);
        void StopSeries(const std::string& seriesId);
        void DeleteOpenEvent(const std::string& eventId);
        void ReportOpenEvent(const std::string& eventId, const std::string& reason);
        void CancelOpen(const std::string& eventId);
        void CancelGuild(const std::string& eventId);

        // ---- recruitment

        /// the guilds that are recruiting. the list is for everybody, so it is asked without the token
        RecruitList RecruitingGuilds(const RecruitFilter& f, int limit = 30, int offset = 0);

        /// the page of a guild, with the draft and the rights of the player when they manage it
        std::optional<RecruitPage> RecruitmentPage(const std::string& guildId);
        std::vector<MyRecruitGuild> MyRecruitmentGuilds();
        std::optional<RecruitPage> SaveRecruitment(const std::string& guildId, const RecruitDraft& draft);
        std::optional<RecruitPage> SetRecruitmentPublished(const std::string& guildId, bool published);
        void ReportRecruitment(const std::string& guildId, const std::string& reason);
        std::vector<Recruiter> GuildRecruiters(const std::string& guildId);
        std::vector<Recruiter> GuildMembers(const std::string& guildId);
        void GrantRecruiter(const std::string& guildId, const std::string& userId);
        void RevokeRecruiter(const std::string& guildId, const std::string& userId);
        void DeleteRecruitment(const std::string& guildId);

        // ---- the wire, open for the tests

        /// [{"role":"healer","slots":2}, ...] in the order of the roles of the form, without the ones at zero
        static Json RolesJson(const std::map<std::string, int>& roles);

    private:
        Json Call(const std::string& fn, Json args);
        Json Rpc(const std::string& fn, const Json& body);

        std::shared_ptr<IHttp> m_http;
        std::string m_key, m_base;
        mutable std::mutex m_lock;  // the token is read by the worker and changed by the window
        std::string m_token;
    };
}
