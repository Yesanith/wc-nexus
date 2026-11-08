#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Time.h"

namespace wc {

    /// keeps the keys of an object in the order the server wrote them, which is the order the roles of a guild event are shown in
    using Json = nlohmann::ordered_json;

    /// a text of a json object, or nothing when it is missing or null. a number is given as its text
    std::optional<std::string> Str(const Json& j, const char* key);
    std::string StrOr(const Json& j, const char* key, const std::string& fallback = "");
    std::optional<int> Int(const Json& j, const char* key);
    bool Bool(const Json& j, const char* key);
    std::vector<std::string> Strings(const Json& j, const char* key);

    /// a game account of the player as the server knows it
    struct AccountInfo {
        std::string id, name, region;
    };

    /// a guild the player is in, with the account that is in it
    struct GuildInfo {
        std::string guildId, name, tag, role, accountId, accountName;

        /// what the server says: leaders and officers, and the members they let. missing from an older server
        std::optional<bool> canCreate;

        /// whether events can be made with this account; without word from the server, leaders and officers
        bool CanCreate() const { return canCreate.value_or(role == "leader" || role == "officer"); }
        std::string Label() const { return tag.empty() ? name : name + " [" + tag + "]"; }
    };

    struct MeInfo {
        std::string name, avatarUrl;
        std::vector<AccountInfo> accounts;
        std::vector<GuildInfo> guilds;

        static MeInfo Parse(const Json& j);
    };

    /// a place of an event for one role, and how many are in it
    struct RolePlace {
        std::string role;
        int slots = 0;
        int taken = 0;
        bool Full() const { return taken >= slots; }
    };

    /// a public or a guild event, the part they share
    struct EventInfo {
        std::string id;
        bool isGuild = false;
        std::string guildId, guildLabel, title, description, activity = "other", language = "any", region = "all";
        Moment startAt = 0;
        int durationMinutes = 0;
        std::optional<int> maxParticipants;
        std::string organizer;
        bool mine = false;
        int going = 0, waitlist = 0, maybe = 0;
        std::vector<std::string> myFlex;
        std::string myStatus, myRole, status = "scheduled";
        std::vector<RolePlace> roles;
        std::vector<std::string> tags;
        std::optional<int> mapId;
        int version = 0;
        std::string seriesId;
        bool canManage = false;

        Moment EndAt() const { return startAt + 60 * (durationMinutes > 0 ? durationMinutes : 0); }

        /// the event is over: nobody can sign up for it or leave it, and it is not changed or cancelled any more
        bool Ended(Moment now) const { return EndAt() <= now; }

        /// the player may change it: they made it, or their rank in the guild allows it
        bool CanChange() const { return mine || canManage; }

        bool Joined() const { return myStatus == "going" || myStatus == "maybe" || myStatus == "waitlist"; }

        /// "4/10", or "4" for an event without a limit
        std::string Places() const;

        static EventInfo ParseOpen(const Json& j);
        static EventInfo ParseGuild(const Json& j);
    };

    /// who is signed up to an event
    struct RosterEntry {
        std::string name, status, role, profession;
        std::vector<std::string> flex;

        static std::vector<RosterEntry> ParseAll(const Json& j);
    };

    /// a guild of the list of the ones that are recruiting
    struct RecruitCard {
        std::string guildId, name, tag, headline, region = "any", playTimes;
        std::vector<std::string> lookingFor, languages;
        bool recruiting = true;

        std::string Title() const { return tag.empty() ? name : name + " [" + tag + "]"; }
        static RecruitCard Parse(const Json& j);
    };

    struct RecruitList {
        std::vector<RecruitCard> rows;
        int total = 0;
        static RecruitList Parse(const Json& j);
    };

    /// the page of a guild, and what the one who asks may do with it
    struct RecruitPage {
        std::string guildId, name, tag, headline, description, requirements, playTimes, region = "any", discordInvite, contact, hiddenReason;
        std::vector<std::string> lookingFor, languages;
        bool recruiting = true, canEdit = false, published = false, hidden = false;

        std::string Title() const { return tag.empty() ? name : name + " [" + tag + "]"; }

        /// nothing when the server had no page to give
        static std::optional<RecruitPage> Parse(const Json& j);
    };

    /// a guild whose page the player may manage, and where the page is
    struct MyRecruitGuild {
        std::string guildId, name, tag;
        bool isLeader = false, hasPage = false, published = false, hidden = false, stale = false;

        std::string Title() const { return tag.empty() ? name : name + " [" + tag + "]"; }
        std::string Status() const;
        static std::vector<MyRecruitGuild> ParseAll(const Json& j);
    };

    /// a member of a guild, or one who is allowed to write its page
    struct Recruiter {
        std::string userId, accountName, role;
        static std::vector<Recruiter> ParseAll(const Json& j);
    };

    /// what is asked of the list of public events: each filter that is empty is not sent
    struct EventFilter {
        std::string activity, language, region, tag;
    };

    /// what is asked of the list of the guilds that recruit. an empty filter is not sent
    struct RecruitFilter {
        std::string language, region, lookingFor, search;
        bool openOnly = true;
    };
}
