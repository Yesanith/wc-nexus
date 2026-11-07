#include "Models.h"

#include "Text.h"

namespace wc {

    std::optional<std::string> Str(const Json& j, const char* key) {
        if (!j.is_object()) return std::nullopt;
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) return std::nullopt;
        if (it->is_string()) return it->get<std::string>();
        if (it->is_number_integer()) return std::to_string(it->get<long long>());
        if (it->is_boolean()) return it->get<bool>() ? std::string("true") : std::string("false");
        return it->dump();
    }

    std::string StrOr(const Json& j, const char* key, const std::string& fallback) {
        auto s = Str(j, key);
        return s ? *s : fallback;
    }

    std::optional<int> Int(const Json& j, const char* key) {
        if (!j.is_object()) return std::nullopt;
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) return std::nullopt;
        if (it->is_number()) return static_cast<int>(it->get<double>());
        if (it->is_string()) {
            try {
                size_t used = 0;
                int n = std::stoi(it->get<std::string>(), &used);
                if (used == it->get<std::string>().size()) return n;
            } catch (...) {
            }
        }
        return std::nullopt;
    }

    bool Bool(const Json& j, const char* key) {
        if (!j.is_object()) return false;
        auto it = j.find(key);
        return it != j.end() && it->is_boolean() && it->get<bool>();
    }

    std::vector<std::string> Strings(const Json& j, const char* key) {
        std::vector<std::string> list;
        if (!j.is_object()) return list;
        auto it = j.find(key);
        if (it == j.end() || !it->is_array()) return list;
        for (const auto& v : *it) {
            if (v.is_string()) list.push_back(v.get<std::string>());
        }
        return list;
    }

    namespace {
        Moment Time(const Json& j, const char* key) {
            auto s = Str(j, key);
            if (!s) return 0;
            auto at = Clock::ParseIso(*s);
            return at ? *at : 0;
        }

        bool RecruitingFlag(const Json& j) {
            // a page that does not say is recruiting, as on the website
            if (!j.is_object()) return true;
            auto it = j.find("recruiting");
            return !(it != j.end() && it->is_boolean() && !it->get<bool>());
        }
    }

    MeInfo MeInfo::Parse(const Json& j) {
        MeInfo me;
        me.name = StrOr(j, "name");
        me.avatarUrl = StrOr(j, "avatar_url");
        if (j.is_object() && j.contains("accounts") && j["accounts"].is_array()) {
            for (const auto& a : j["accounts"]) me.accounts.push_back({StrOr(a, "id"), StrOr(a, "name"), StrOr(a, "region")});
        }
        if (j.is_object() && j.contains("guilds") && j["guilds"].is_array()) {
            for (const auto& g : j["guilds"]) {
                me.guilds.push_back({StrOr(g, "guild_id"), StrOr(g, "name"), StrOr(g, "tag"), StrOr(g, "role"), StrOr(g, "account_id"), StrOr(g, "account_name")});
            }
        }
        return me;
    }

    std::string EventInfo::Places() const {
        return maxParticipants ? std::to_string(going) + "/" + std::to_string(*maxParticipants) : std::to_string(going);
    }

    EventInfo EventInfo::ParseOpen(const Json& j) {
        EventInfo e;
        e.id = StrOr(j, "id");
        e.isGuild = false;
        e.title = StrOr(j, "title");
        e.description = StrOr(j, "description");
        e.activity = StrOr(j, "activity", "other");
        e.language = StrOr(j, "language", "any");
        e.region = StrOr(j, "region", "all");
        e.startAt = Time(j, "start_at");
        e.durationMinutes = Int(j, "duration_minutes").value_or(0);
        e.maxParticipants = Int(j, "max_participants");
        e.organizer = StrOr(j, "organizer");
        e.mine = Bool(j, "mine");
        e.going = Int(j, "going").value_or(0);
        e.maybe = Int(j, "maybe").value_or(0);
        e.myStatus = StrOr(j, "my_status");
        e.myRole = StrOr(j, "my_role");
        e.status = StrOr(j, "status", "scheduled");
        e.mapId = Int(j, "map_id");
        e.tags = Strings(j, "tags");
        if (j.is_object() && j.contains("roles") && j["roles"].is_array()) {
            for (const auto& r : j["roles"]) e.roles.push_back({StrOr(r, "role"), Int(r, "slots").value_or(0), Int(r, "taken").value_or(0)});
        }
        return e;
    }

    EventInfo EventInfo::ParseGuild(const Json& j) {
        EventInfo e;
        std::string tag = StrOr(j, "guild_tag"), name = StrOr(j, "guild_name");
        e.id = StrOr(j, "id");
        e.isGuild = true;
        e.guildId = StrOr(j, "guild_id");
        e.guildLabel = tag.empty() ? name : name + " [" + tag + "]";
        e.title = StrOr(j, "title");
        e.description = StrOr(j, "description");
        e.activity = StrOr(j, "activity", "other");
        e.startAt = Time(j, "start_at");
        e.durationMinutes = Int(j, "duration_minutes").value_or(0);
        e.maxParticipants = Int(j, "max_participants");
        e.organizer = StrOr(j, "organizer");
        e.mine = Bool(j, "mine");
        e.going = Int(j, "going").value_or(0);
        e.waitlist = Int(j, "waitlist").value_or(0);
        e.maybe = Int(j, "maybe").value_or(0);
        e.myFlex = Strings(j, "my_flex");
        e.myStatus = StrOr(j, "my_status");
        e.myRole = StrOr(j, "my_role");
        e.status = StrOr(j, "status", "scheduled");
        e.version = Int(j, "version").value_or(0);
        e.seriesId = StrOr(j, "series_id");
        e.canManage = Bool(j, "can_manage");
        // the guild events name the places as { role: slots }, without how many are taken
        if (j.is_object() && j.contains("roles") && j["roles"].is_object()) {
            for (auto it = j["roles"].begin(); it != j["roles"].end(); ++it) {
                int slots = it->is_number() ? static_cast<int>(it->get<double>()) : 0;
                e.roles.push_back({it.key(), slots, 0});
            }
        }
        return e;
    }

    std::vector<RosterEntry> RosterEntry::ParseAll(const Json& j) {
        std::vector<RosterEntry> list;
        if (!j.is_array()) return list;
        for (const auto& r : j) {
            list.push_back({StrOr(r, "name"), StrOr(r, "status"), StrOr(r, "role"), StrOr(r, "profession"), Strings(r, "flex_roles")});
        }
        return list;
    }

    RecruitCard RecruitCard::Parse(const Json& j) {
        RecruitCard c;
        c.guildId = StrOr(j, "guild_id");
        c.name = StrOr(j, "name");
        c.tag = StrOr(j, "tag");
        c.headline = StrOr(j, "headline");
        c.lookingFor = Strings(j, "looking_for");
        c.languages = Strings(j, "languages");
        c.region = StrOr(j, "region", "any");
        c.recruiting = RecruitingFlag(j);
        c.playTimes = StrOr(j, "play_times");
        return c;
    }

    RecruitList RecruitList::Parse(const Json& j) {
        RecruitList list;
        list.total = Int(j, "total").value_or(0);
        if (j.is_object() && j.contains("rows") && j["rows"].is_array()) {
            for (const auto& r : j["rows"]) {
                RecruitCard card = RecruitCard::Parse(r);
                if (!card.guildId.empty() && !card.name.empty()) list.rows.push_back(card);
            }
        }
        return list;
    }

    std::optional<RecruitPage> RecruitPage::Parse(const Json& j) {
        if (!j.is_object()) return std::nullopt;
        RecruitPage p;
        p.guildId = StrOr(j, "guild_id");
        p.name = StrOr(j, "name");
        p.tag = StrOr(j, "tag");
        p.headline = StrOr(j, "headline");
        p.description = StrOr(j, "description");
        p.requirements = StrOr(j, "requirements");
        p.playTimes = StrOr(j, "play_times");
        p.lookingFor = Strings(j, "looking_for");
        p.languages = Strings(j, "languages");
        p.region = StrOr(j, "region", "any");
        p.recruiting = RecruitingFlag(j);
        p.discordInvite = StrOr(j, "discord_invite");
        p.contact = StrOr(j, "contact");
        p.canEdit = Bool(j, "can_edit");
        p.published = Bool(j, "published");
        p.hidden = Bool(j, "hidden");
        p.hiddenReason = StrOr(j, "hidden_reason");
        return p;
    }

    std::string MyRecruitGuild::Status() const {
        return Loc::T(!hasPage ? "No page yet" : hidden ? "Hidden by a moderator" : !published ? "Draft" : stale ? "Not shown: not saved for 60 days" : "Shown");
    }

    std::vector<MyRecruitGuild> MyRecruitGuild::ParseAll(const Json& j) {
        std::vector<MyRecruitGuild> list;
        if (!j.is_array()) return list;
        for (const auto& r : j) {
            std::string id = StrOr(r, "guild_id");
            if (id.empty()) continue;
            MyRecruitGuild g;
            g.guildId = id;
            g.name = StrOr(r, "name");
            g.tag = StrOr(r, "tag");
            g.isLeader = Bool(r, "is_leader");
            g.hasPage = Bool(r, "has_page");
            g.published = Bool(r, "published");
            g.hidden = Bool(r, "hidden");
            g.stale = Bool(r, "stale");
            list.push_back(g);
        }
        return list;
    }

    std::vector<Recruiter> Recruiter::ParseAll(const Json& j) {
        std::vector<Recruiter> list;
        if (!j.is_array()) return list;
        for (const auto& r : j) {
            std::string id = StrOr(r, "user_id");
            if (!id.empty()) list.push_back({id, StrOr(r, "account_name"), StrOr(r, "role")});
        }
        return list;
    }
}
