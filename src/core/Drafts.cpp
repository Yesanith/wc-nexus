#include "Drafts.h"

#include <algorithm>
#include <cstdlib>
#include <regex>

#include "Rules.h"
#include "Text.h"

namespace wc {

    namespace {
        /// a whole number and nothing else, or nothing
        std::optional<int> Number(const std::string& text) {
            std::string s = Trimmed(text);
            if (s.empty() || s.size() > 9) return std::nullopt;
            for (char c : s) {
                if (c < '0' || c > '9') return std::nullopt;
            }
            return std::atoi(s.c_str());
        }
    }

    int EventDraft::RoleTotal() const {
        int n = 0;
        for (const auto& kv : roles) n += kv.second;
        return n;
    }

    std::optional<Moment> EventDraft::When(Moment now) const { return TimeInput::Parse(whenText, now); }

    std::string EventDraft::Validate(Moment now) const {
        size_t titleLength = Length(Trimmed(title));
        if (titleLength < 3 || titleLength > 80) return Loc::T("The title has to be 3 to 80 characters.");
        auto when = When(now);
        if (!when) return Loc::T("I could not read the time. Try 20:00, +1d 20:00 or 2026-12-24 19:00.");
        // an event that is changed may already have begun: the server says whether that is allowed
        if (!IsEdit() && *when <= now) return Loc::T("The start must be in the future.");
        if (Length(Trimmed(description)) > 1000) return Loc::T("The notes can be 1000 characters at most.");
        if (roles.empty() && !Trimmed(maxText).empty()) {
            auto max = Number(maxText);
            if (!max || *max < 2 || *max > 100) return Loc::T("The places have to be 2 to 100.");
        }
        if (!IsGuild()) {
            if (!Trimmed(mapText).empty()) {
                auto map = Number(mapText);
                if (!map || *map < 1 || *map > 100000) return Loc::T("The map id has to be a number from 1 to 100000.");
            }
            if (!roles.empty() && !IsEdit()) {
                if (myRole.empty() || roles.find(myRole) == roles.end()) return Loc::T("Pick the role you come as.");
                if (EventRules::ProfessionOrEmpty(myProfession).empty()) return Loc::T("Pick your profession.");
            }
        } else if (accountId.empty() && !IsEdit()) {
            return Loc::T("Pick the game account you organize with.");
        }
        return "";
    }

    std::optional<int> EventDraft::MaxParticipants() const {
        if (!roles.empty()) return RoleTotal();
        return Number(maxText);
    }

    EventDraft EventDraft::FromEvent(const EventInfo& e) {
        EventDraft d;
        d.guildId = e.isGuild ? e.guildId : "";
        d.title = e.title;
        d.activity = e.activity.empty() ? "other" : e.activity;
        d.language = e.language.empty() ? "any" : e.language;
        d.region = e.region.empty() ? "all" : e.region;
        d.mapText = e.mapId ? std::to_string(*e.mapId) : "";
        d.whenText = Clock::Typed(e.startAt);
        // the length the event has, also one the form does not offer: changing the title is not changing the length
        if (e.durationMinutes > 0) d.durationMinutes = e.durationMinutes;
        d.description = e.description;
        d.editId = e.id;
        d.editVersion = e.version;
        d.editSeriesId = e.seriesId;
        d.maxText = e.roles.empty() && e.maxParticipants ? std::to_string(*e.maxParticipants) : "";
        for (const auto& t : e.tags) d.tags.insert(t);
        for (const auto& r : e.roles) {
            if (r.slots > 0) d.roles[r.role] = r.slots;
        }
        return d;
    }

    NewOpenEvent EventDraft::ToOpen(Moment now) const {
        NewOpenEvent o;
        o.title = Trimmed(title);
        o.startAt = When(now).value_or(0);
        o.durationMinutes = durationMinutes;
        o.description = Trimmed(description);
        o.activity = activity;
        o.language = language;
        o.maxParticipants = roles.empty() ? MaxParticipants() : std::nullopt;
        o.region = region;
        o.roles = roles;
        o.myRole = roles.empty() ? "" : myRole;
        o.myProfession = roles.empty() ? "" : myProfession;
        o.tags.assign(tags.begin(), tags.end());
        o.mapId = Number(mapText);
        return o;
    }

    NewGuildEvent EventDraft::ToGuild(Moment now) const {
        NewGuildEvent g;
        g.guildId = guildId;
        g.title = Trimmed(title);
        g.startAt = When(now).value_or(0);
        g.durationMinutes = durationMinutes;
        g.description = Trimmed(description);
        g.activity = activity;
        g.maxParticipants = MaxParticipants();
        g.roles = roles;
        g.accountId = accountId;
        return g;
    }

    void EventDraft::UsePreset(const std::string& name) {
        roles = EventRules::Preset(name);
        if (!myRole.empty() && roles.find(myRole) == roles.end()) myRole.clear();
    }

    void EventDraft::ChangeRole(const std::string& role, int delta) {
        auto it = roles.find(role);
        int n = std::max(0, std::min(50, (it == roles.end() ? 0 : it->second) + delta));
        if (n == 0) roles.erase(role);
        else roles[role] = n;
        if (!myRole.empty() && roles.find(myRole) == roles.end()) myRole.clear();
    }

    bool EventDraft::ToggleTag(const std::string& tag, bool on) {
        if (!on) return tags.erase(tag) > 0;
        if (tags.count(tag)) return false;
        if (static_cast<int>(tags.size()) >= EventRules::MaxTags) return false;
        tags.insert(tag);
        return true;
    }

    RecruitDraft RecruitDraft::From(const std::optional<RecruitPage>& page) {
        RecruitDraft d;
        if (!page) return d;
        d.headline = page->headline;
        d.description = page->description;
        d.requirements = page->requirements;
        d.playTimes = page->playTimes;
        d.contact = page->contact;
        d.discordInvite = page->discordInvite;
        d.region = page->region;
        d.recruiting = page->recruiting;
        d.lookingFor.insert(page->lookingFor.begin(), page->lookingFor.end());
        d.languages.insert(page->languages.begin(), page->languages.end());
        return d;
    }

    bool RecruitDraft::ToggleLookingFor(const std::string& key, bool on) {
        if (!on) return lookingFor.erase(key) > 0;
        if (lookingFor.count(key)) return false;
        if (static_cast<int>(lookingFor.size()) >= RecruitRules::LookingForMax) return false;
        lookingFor.insert(key);
        return true;
    }

    void RecruitDraft::SetLanguage(const std::string& code) {
        languages.clear();
        if (!code.empty()) languages.insert(code);
    }

    std::string RecruitDraft::Validate(bool publishing) const {
        if (Length(Trimmed(headline)) > RecruitRules::HeadlineMax) return Loc::T("The headline can be 80 characters at most.");
        if (Length(Trimmed(description)) > RecruitRules::DescriptionMax) return Loc::T("The description can be 2000 characters at most.");
        if (Length(Trimmed(requirements)) > RecruitRules::RequirementsMax) return Loc::T("The requirements can be 500 characters at most.");
        if (Length(Trimmed(playTimes)) > RecruitRules::PlayTimesMax) return Loc::T("When you play can be 120 characters at most.");
        if (Length(Trimmed(contact)) > RecruitRules::ContactMax) return Loc::T("The contact can be 120 characters at most.");
        std::string invite = Trimmed(discordInvite);
        if (!invite.empty() && !IsDiscordInvite(invite)) {
            return Loc::T("The Discord invite has to look like https://discord.gg/abc or https://discord.com/invite/abc.");
        }
        if (static_cast<int>(lookingFor.size()) > RecruitRules::LookingForMax) return Loc::T("Pick four things at the most that you look for.");
        if (static_cast<int>(languages.size()) > RecruitRules::LanguagesMax) return Loc::T("A page plays in one language: pick one.");
        if (publishing) {
            if (Trimmed(headline).empty()) return Loc::T("A page that is shown needs a headline.");
            if (Length(Trimmed(description)) < static_cast<size_t>(RecruitRules::DescriptionMin)) return Loc::T("The description needs 20 characters at least.");
            if (invite.empty() && Trimmed(contact).empty()) return Loc::T("Give a way to reach the guild: a Discord invite or a name to whisper.");
        }
        return "";
    }

    Json RecruitDraft::ToJson() const {
        std::string invite = Trimmed(discordInvite);
        if (!invite.empty() && invite.back() == '/') invite.pop_back();
        Json looking = Json::array(), langs = Json::array();
        for (const auto& t : RecruitRules::Tags) {
            if (lookingFor.count(t)) looking.push_back(t);
        }
        for (const auto& l : RecruitRules::Languages) {
            if (languages.count(l)) langs.push_back(l);
        }
        Json j = Json::object();
        j["headline"] = Trimmed(headline);
        j["description"] = Trimmed(description);
        j["requirements"] = Trimmed(requirements);
        j["play_times"] = Trimmed(playTimes);
        j["contact"] = Trimmed(contact);
        j["discord_invite"] = invite.empty() ? Json(nullptr) : Json(invite);
        j["region"] = region;
        j["recruiting"] = recruiting;
        j["looking_for"] = looking;
        j["languages"] = langs;
        return j;
    }

    bool IsDiscordInvite(const std::string& text) {
        // what the database takes, and a slash at the end, which is taken off before it is sent
        static const std::regex invite(R"(^https://(discord\.gg|discord\.com/invite)/[A-Za-z0-9-]{2,40}/?$)");
        return std::regex_match(text, invite);
    }
}
