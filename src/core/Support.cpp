#include "Support.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

#include "Rules.h"
#include "Text.h"

namespace wc {

    namespace {
        const std::map<std::string, const char*>& Known() {
            static const std::map<std::string, const char*> known = {
                {"invalid_token", "This module is no longer linked. Get a new code in the app."},
                {"not_a_member", "You are not a member of that guild."},
                {"not_allowed", "Your guild rank does not allow that."},
                {"forbidden", "You are not allowed to do that."},
                {"role_full", "That role is full."},
                {"role_required", "Pick a role first."},
                {"profession_required", "Your profession is not known yet. Pick a character in the game first."},
                {"bad_profession", "That profession cannot be used."},
                {"wrong_region", "None of your game accounts is in the region of this event."},
                {"event_full", "The event is full."},
                {"too_many_events", "You have made too many events for now."},
                {"too_many_open_events", "You have made too many public events for now."},
                {"bad_title", "The title is not valid."},
                {"bad_start", "The start must be in the future."},
                {"start_in_past", "The start must be in the future."},
                {"bad_activity", "That activity cannot be used."},
                {"bad_roles", "The roles are not valid."},
                {"no_account", "Link a game account in the app first."},
                {"too_many", "Too many tries. Wait a minute."},
                {"incomplete", "The page needs a headline, a description and a way to be reached before it can be shown."},
                {"no_page", "Save the page first."},
                {"hidden", "A moderator hid this page. It cannot be shown until they show it again."},
                {"invalid_field", "One of the fields is not valid."},
                {"invalid_filter", "That filter is not valid."},
                {"own_guild", "You cannot report the page of your own guild."},
                {"event_over", "This event is already over."},
                {"event_cancelled", "This event was cancelled."},
                {"event_not_found", "That event is not there any more."},
                {"discord_only", "This event is signed up for in Discord."},
                {"role_restricted", "This event is open to certain Discord roles only."},
                {"account_not_allowed", "That account is not yours in this guild."},
                {"account_taken", "That account is already in this."},
                {"banned", "You may not make public events."},
                {"below_going", "More players have signed up than the places you gave it."},
                {"bad_duration", "The length has to be between 15 minutes and 12 hours."},
                {"bad_map", "The map id is not valid."},
                {"bad_tags", "The tags are not valid."},
                {"bad_language", "That language cannot be used."},
                {"bad_region", "That region cannot be used."},
                {"bad_capacity", "The places have to be 2 to 100."},
                {"bad_description", "The notes are too long."},
                {"bad_role", "That role cannot be used."},
                {"bad_flex", "Those spare roles cannot be used."},
                {"bad_status", "That is not a way to sign up."},
                {"start_too_far", "The start cannot be more than 90 days away."},
                {"too_many_open", "You have too many public events open. Wait for one to end."},
                {"too_many_today", "You made too many events today."},
                {"too_many_reports", "You reported too many pages today."},
                {"too_many_links", "Ten modules are linked to your account. Unlink one in the app first."},
                {"too_many_codes", "Five codes are waiting. Use one or wait ten minutes."},
                {"already_reported", "You reported this already."},
                {"not_signed_up", "You are not signed up for this event."},
                {"not_leader", "Only the leader of the guild can do that."},
                {"not_found", "That is not there any more."},
                {"cannot_create", "You cannot make events for this guild."},
                {"bad_invite", "That is not a Discord invite."},
                {"guild_not_found", "That guild is not there any more."},
                {"unknown_guild", "That guild is not there any more."},
                {"version_conflict", "Somebody changed this event meanwhile. Refresh the list and try again."},
                {"not_creator", "Only the one who made this event can do that."},
                {"event_gone", "That event is not there any more."},
                {"own_event", "You cannot report your own event."},
                {"bad_reason", "That is not a reason."},
                {"not_member", "You are not a member of that guild."},
                {"own_role", "The leader needs no permission."},
                {"is_leader", "The leader needs no permission."},
                {"42501", "You are not allowed to do that."},
                {"bad_code", "That code is wrong or has run out. Get a new one in the app."},
                // the limits per player on writes that are repeated: sign ups, reports, changes of an event, saves of a page
                {"rate_limited", "That was a lot of times in a short while. Wait a little and try again."},
            };
            return known;
        }
    }

    namespace ErrorText {
        std::string For(const ApiError& e) {
            if (e.IsNetwork()) return Loc::T("Could not reach the server. Check the connection.");
            auto it = Known().find(e.code);
            if (it != Known().end()) return Loc::T(it->second);
            if (e.code == "PGRST202") return Loc::T("The server does not have this yet. It may still be updating, try again in a few minutes.");
            if (e.status >= 500) return Loc::T("The server had a problem. Try again in a moment.");
            return Loc::T("That did not work ({0}).", {e.code});
        }

        std::string ForUnknown() { return Loc::T("Something went wrong."); }

        std::string ForLink(const std::string& error) {
            auto it = Known().find(error);
            return it != Known().end() ? Loc::T(it->second) : Loc::T("That did not work.");
        }

        std::vector<std::string> AllTexts() {
            std::vector<std::string> texts;
            for (const auto& kv : Known()) texts.push_back(kv.second);
            return texts;
        }
    }

    std::vector<EventInfo> ReminderPlanner::Due(const std::vector<EventInfo>& events, Moment now, int minutes) {
        std::vector<EventInfo> due;
        long long window = 60LL * std::max(1, minutes);
        for (const auto& e : events) {
            if (e.id.empty() || e.status == "cancelled") continue;
            if (!(e.myStatus == "going" || e.myStatus == "maybe" || e.mine)) continue;
            long long until = static_cast<long long>(e.startAt) - static_cast<long long>(now);
            if (until > window || until < -120) continue;
            if (!m_told.insert(e.id).second) continue;
            due.push_back(e);
        }
        std::stable_sort(due.begin(), due.end(), [](const EventInfo& a, const EventInfo& b) { return a.startAt < b.startAt; });
        return due;
    }

    void ReminderPlanner::Forget() {
        if (m_told.size() > 500) m_told.clear();
    }

    std::string ReminderPlanner::Message(const EventInfo& e, Moment now) {
        long long until = static_cast<long long>(e.startAt) - static_cast<long long>(now);
        if (until <= 30) return Loc::T("{0} is starting now", {e.title});
        long long minutes = (until + 59) / 60;
        return minutes == 1 ? Loc::T("{0} starts in {1} minute", {e.title, std::to_string(minutes)})
                            : Loc::T("{0} starts in {1} minutes", {e.title, std::to_string(minutes)});
    }

    TokenStore::TokenStore(std::filesystem::path directory) : m_path(std::move(directory) / "link.token") {}

    std::string TokenStore::Load() const {
        std::ifstream in(m_path, std::ios::binary);
        if (!in) return "";
        std::stringstream ss;
        ss << in.rdbuf();
        std::string token = Trimmed(ss.str());
        return IsToken(token) ? token : "";
    }

    bool TokenStore::Save(const std::string& token) const {
        if (!IsToken(token)) return false;
        std::error_code ec;
        std::filesystem::create_directories(m_path.parent_path(), ec);
        auto tmp = m_path;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out << token;
            if (!out) return false;
        }
        std::filesystem::rename(tmp, m_path, ec);
        if (ec) {
            std::filesystem::remove(m_path, ec);
            std::filesystem::rename(tmp, m_path, ec);
        }
        return !ec;
    }

    void TokenStore::Clear() const {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    bool TokenStore::IsToken(const std::string& s) {
        if (s.size() != 68 || s.compare(0, 4, "tcm_") != 0) return false;
        return std::all_of(s.begin() + 4, s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
    }

    void Settings::Clamp() {
        reminderMinutes = std::max(1, std::min(60, reminderMinutes));
        refreshMinutes = std::max(1, std::min(30, refreshMinutes));
        windowSize = std::max(60, std::min(200, windowSize));
        const auto& codes = Loc::Codes();
        if (language != "auto" && std::find(codes.begin(), codes.end(), language) == codes.end()) language = "auto";
    }

    Settings Settings::Load(const std::filesystem::path& directory) {
        Settings s;
        std::ifstream in(directory / "settings.json", std::ios::binary);
        if (!in) return s;
        std::stringstream ss;
        ss << in.rdbuf();
        Json j = Json::parse(ss.str(), nullptr, false);
        if (j.is_discarded() || !j.is_object()) return s;
        s.language = StrOr(j, "language", s.language);
        if (j.contains("reminders") && j["reminders"].is_boolean()) s.reminders = j["reminders"].get<bool>();
        s.reminderMinutes = Int(j, "reminder_minutes").value_or(s.reminderMinutes);
        s.refreshMinutes = Int(j, "refresh_minutes").value_or(s.refreshMinutes);
        s.windowSize = Int(j, "window_size").value_or(s.windowSize);
        s.defaultAccountId = StrOr(j, "default_account");
        s.Clamp();
        return s;
    }

    bool Settings::Save(const std::filesystem::path& directory) const {
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        Json j = Json::object();
        j["language"] = language;
        j["reminders"] = reminders;
        j["reminder_minutes"] = reminderMinutes;
        j["refresh_minutes"] = refreshMinutes;
        j["window_size"] = windowSize;
        j["default_account"] = defaultAccountId;
        std::ofstream out(directory / "settings.json", std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << j.dump(1);
        return static_cast<bool>(out);
    }

    std::string PublicEventLink(const std::string& id) { return "https://yesanith.github.io/wc-frontend/app/#/event/" + id; }
}
