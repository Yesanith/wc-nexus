#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wc {

    /// the values the backend accepts for events, the ready made role sets, and each of them in words
    namespace EventRules {
        extern const std::vector<std::string> OpenActivities;
        extern const std::vector<std::string> GuildActivities;
        extern const std::vector<std::string> Languages;  // "any" first
        extern const std::vector<std::string> Regions;    // "all", "eu", "na"
        extern const std::vector<std::string> RoleKeys;
        extern const std::vector<std::string> Tags;
        extern const std::vector<int> Durations;
        extern const std::vector<std::string> Professions;
        extern const std::vector<std::string> PresetNames;  // the English names the sets are known by
        constexpr int MaxTags = 3;

        std::map<std::string, int> Preset(const std::string& name);

        std::string TagLabel(const std::string& key);
        std::string RegionLabel(const std::string& key);
        std::string DurationLabel(int minutes);
        std::string StatusLabel(const std::string& status);
        std::string RankLabel(const std::string& rank);
        std::string PresetLabel(const std::string& name);
        std::string RoleLabel(const std::string& key);
        std::string ActivityLabel(const std::string& key);

        /// the places a role set makes, which is the size of the group; nothing for no roles
        std::optional<int> Size(const std::map<std::string, int>& roles);

        /// the profession as the api names it ("Guardian"), when the name is one an event takes, in any case of letters; empty otherwise
        std::string ProfessionOrEmpty(const std::string& name);

        /// the profession of a character by the number the game gives it (1 Guardian to 9 Revenant); empty for another
        std::string ProfessionOfId(int id);
    }

    /// what the recruitment pages of the guilds may say, as the database knows it
    namespace RecruitRules {
        extern const std::vector<std::string> Tags;
        extern const std::vector<std::string> Languages;
        extern const std::vector<std::string> Regions;  // "any", "eu", "na"

        constexpr int HeadlineMax = 80;
        constexpr int DescriptionMax = 2000;
        constexpr int DescriptionMin = 20;
        constexpr int RequirementsMax = 500;
        constexpr int PlayTimesMax = 120;
        constexpr int ContactMax = 120;

        /// what a page looks for and the language it plays in are the tags of its post in the forum, which carries five at the most:
        /// one language and four things it looks for
        constexpr int TagsMax = 5;
        constexpr int LanguagesMax = 1;
        constexpr int LookingForMax = TagsMax - LanguagesMax;

        std::string TagLabel(const std::string& key);
        std::string LanguageLabel(const std::string& code);
        std::string RegionLabel(const std::string& region);
    }

    /// the letters of a text, counted as a player counts them (code points of UTF-8), as the limits of the server count them
    size_t Length(const std::string& utf8);

    /// a text without the spaces at its ends
    std::string Trimmed(const std::string& s);

    /// a text cut to [max] letters with an ellipsis, on one line
    std::string Shorten(const std::string& text, size_t max);
}
