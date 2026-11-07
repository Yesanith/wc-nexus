#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>

#include "Models.h"

namespace wc {

    struct NewOpenEvent {
        std::string title;
        Moment startAt = 0;
        int durationMinutes = 60;
        std::string description, activity = "other", language = "any";
        std::optional<int> maxParticipants;
        std::string accountId, region = "all";
        std::map<std::string, int> roles;
        std::string myRole, myProfession;
        std::vector<std::string> tags;
        std::optional<int> mapId;
    };

    struct NewGuildEvent {
        std::string guildId, title;
        Moment startAt = 0;
        int durationMinutes = 60;
        std::string description, activity = "other";
        std::optional<int> maxParticipants;
        std::string accountId;
        std::map<std::string, int> roles;
    };

    /// the form of a new event, the fields the website has. it says what is wrong with it before anything is sent
    struct EventDraft {
        /// empty makes a public event, a guild id makes a guild event
        std::string guildId, accountId;

        std::string title, activity = "raid", region = "all", language = "any";
        std::set<std::string> tags;
        std::string mapText, whenText;
        int durationMinutes = 120;
        bool weekly = false;
        std::map<std::string, int> roles;
        std::string maxText, myRole, myProfession, description;

        /// the event that is being changed, when the draft is one: its id, the version it was read at, and the weekly series it is of
        std::string editId;
        int editVersion = 0;
        std::string editSeriesId;

        bool IsEdit() const { return !editId.empty(); }
        bool IsGuild() const { return !guildId.empty(); }
        int RoleTotal() const;

        /// what is wrong with the draft, in words for the player, or empty when it can be sent
        std::string Validate(Moment now) const;

        std::optional<Moment> When(Moment now) const;

        /// with roles the places are those of the roles. otherwise the number typed, or open
        std::optional<int> MaxParticipants() const;

        /// the form of an event that is already there, to be changed: what it says now, with the time as it is typed
        static EventDraft FromEvent(const EventInfo& e);

        NewOpenEvent ToOpen(Moment now) const;
        NewGuildEvent ToGuild(Moment now) const;

        void UsePreset(const std::string& name);
        void ChangeRole(const std::string& role, int delta);

        /// at most three tags, as the website allows. false when the tag could not be put on
        bool ToggleTag(const std::string& tag, bool on);
    };

    /// the form of a recruitment page, with what the website checks before a page is sent
    struct RecruitDraft {
        std::string headline, description, requirements, playTimes, contact, discordInvite, region = "any";
        bool recruiting = true;
        std::set<std::string> lookingFor, languages;

        static RecruitDraft From(const std::optional<RecruitPage>& page);

        int TagCount() const { return static_cast<int>(lookingFor.size() + languages.size()); }

        /// switches a thing the guild looks for on or off: four at the most, so that the language has its place among the five tags
        bool ToggleLookingFor(const std::string& key, bool on);

        /// the language is one: choosing another takes the first off, and choosing none leaves none
        void SetLanguage(const std::string& code);

        /// what is wrong with the page, or empty. a page that is to be shown also needs a headline, a description and a way to be reached
        std::string Validate(bool publishing) const;

        /// every field is sent, so one that was cleared is cleared on the page
        Json ToJson() const;
    };

    /// whether a text is a Discord invite the server takes: https://discord.gg/abc or https://discord.com/invite/abc
    bool IsDiscordInvite(const std::string& text);
}
