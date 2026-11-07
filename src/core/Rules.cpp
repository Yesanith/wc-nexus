#include "Rules.h"

#include <algorithm>
#include <cctype>

#include "Text.h"

namespace wc {

    namespace {
        std::string Capitalized(const std::string& key) {
            if (key.empty()) return "";
            std::string s = key;
            s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
            return s;
        }

        std::string LowerAscii(std::string s) {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }
    }

    namespace EventRules {
        const std::vector<std::string> OpenActivities = {"raid", "strike", "fractal", "worldboss", "meta", "wvw", "pvp", "dungeon", "other"};
        const std::vector<std::string> GuildActivities = {"raid", "strike", "fractal", "meta", "wvw", "pvp", "dungeon", "other"};
        const std::vector<std::string> Languages = {"any", "en", "de", "es", "fr", "tr", "pt", "ru", "ja", "zh"};
        const std::vector<std::string> Regions = {"all", "eu", "na"};
        const std::vector<std::string> RoleKeys = {"tank", "healer", "boon_dps", "dps", "support"};
        const std::vector<std::string> Tags = {"meta", "casual", "achievement", "training", "speedrun", "new_players"};
        const std::vector<int> Durations = {30, 60, 90, 120, 180, 240, 360};
        const std::vector<std::string> Professions = {"Guardian", "Warrior", "Engineer", "Ranger", "Thief", "Elementalist", "Mesmer", "Necromancer", "Revenant"};
        const std::vector<std::string> PresetNames = {"No roles", "Squad of 10", "Party of 5"};

        std::map<std::string, int> Preset(const std::string& name) {
            if (name == "Squad of 10") return {{"healer", 2}, {"boon_dps", 2}, {"dps", 6}};
            if (name == "Party of 5") return {{"healer", 1}, {"boon_dps", 1}, {"dps", 3}};
            return {};
        }

        std::string TagLabel(const std::string& key) {
            if (key == "meta") return Loc::T("Meta");
            if (key == "casual") return Loc::T("Casual");
            if (key == "achievement") return Loc::T("Achievement");
            if (key == "training") return Loc::T("Training");
            if (key == "speedrun") return Loc::T("Speedrun");
            if (key == "new_players") return Loc::T("New players");
            return ActivityLabel(key);
        }

        std::string RegionLabel(const std::string& key) {
            if (key == "all") return Loc::T("All regions");
            std::string s = key;
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        std::string DurationLabel(int minutes) {
            if (minutes < 60) return Loc::T("{0} min", {std::to_string(minutes)});
            if (minutes % 60 == 0) return Loc::T("{0} h", {std::to_string(minutes / 60)});
            return Loc::T("{0} h {1} min", {std::to_string(minutes / 60), std::to_string(minutes % 60)});
        }

        std::string StatusLabel(const std::string& status) {
            if (status == "going") return Loc::T("going");
            if (status == "maybe") return Loc::T("maybe");
            if (status == "waitlist") return Loc::T("waiting");
            if (status == "declined") return Loc::T("declined");
            return status;
        }

        std::string RankLabel(const std::string& rank) {
            if (rank == "leader") return Loc::T("leader");
            if (rank == "officer") return Loc::T("officer");
            if (rank == "member") return Loc::T("member");
            return rank;
        }

        std::string PresetLabel(const std::string& name) { return Loc::T(name.c_str()); }

        std::string RoleLabel(const std::string& key) {
            if (key == "tank") return Loc::T("Tank");
            if (key == "healer") return Loc::T("Healer");
            if (key == "boon_dps") return Loc::T("Boon DPS");
            if (key == "dps") return Loc::T("DPS");
            if (key == "support") return Loc::T("Support");
            return Capitalized(key);
        }

        std::string ActivityLabel(const std::string& key) {
            if (key == "raid") return Loc::T("Raid");
            if (key == "strike") return Loc::T("Strike");
            if (key == "fractal") return Loc::T("Fractal");
            if (key == "meta") return Loc::T("Meta");
            if (key == "wvw") return Loc::T("WvW");
            if (key == "pvp") return Loc::T("PvP");
            if (key == "worldboss") return Loc::T("World boss");
            if (key == "dungeon") return Loc::T("Dungeon");
            if (key == "other") return Loc::T("Other");
            return Capitalized(key);
        }

        std::optional<int> Size(const std::map<std::string, int>& roles) {
            if (roles.empty()) return std::nullopt;
            int n = 0;
            for (const auto& kv : roles) n += kv.second;
            return n;
        }

        std::string ProfessionOrEmpty(const std::string& name) {
            std::string wanted = LowerAscii(name);
            for (const auto& p : Professions) {
                if (LowerAscii(p) == wanted) return p;
            }
            return "";
        }

        std::string ProfessionOfId(int id) { return id >= 1 && id <= 9 ? Professions[static_cast<size_t>(id - 1)] : ""; }
    }

    namespace RecruitRules {
        const std::vector<std::string> Tags = {"raids", "fractals", "strikes", "dungeons", "open_world", "achievements", "wvw", "pvp",
                                              "farming", "casual", "social", "newcomers", "roleplay"};
        const std::vector<std::string> Languages = {"en", "de", "es", "fr", "tr", "pt", "ru"};
        const std::vector<std::string> Regions = {"any", "eu", "na"};

        std::string TagLabel(const std::string& key) {
            if (key == "open_world") return Loc::T("Open world");
            if (key == "wvw") return Loc::T("WvW");
            if (key == "pvp") return Loc::T("PvP");
            if (key == "newcomers") return Loc::T("Newcomers");
            if (key == "raids") return Loc::T("Raids");
            if (key == "fractals") return Loc::T("Fractals");
            if (key == "strikes") return Loc::T("Strikes");
            if (key == "dungeons") return Loc::T("Dungeons");
            if (key == "achievements") return Loc::T("Achievements");
            if (key == "farming") return Loc::T("Farming");
            if (key == "casual") return Loc::T("Casual");
            if (key == "social") return Loc::T("Social");
            if (key == "roleplay") return Loc::T("Roleplay");
            return Capitalized(key);
        }

        std::string LanguageLabel(const std::string& code) {
            if (code == "en" || code == "de" || code == "es" || code == "fr" || code == "tr" || code == "pt" || code == "ru" || code == "ja" || code == "zh") {
                return Loc::NativeName(code);
            }
            std::string s = code;
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        std::string RegionLabel(const std::string& region) {
            if (region == "eu") return "EU";
            if (region == "na") return "NA";
            return Loc::T("Any region");
        }
    }

    size_t Length(const std::string& utf8) {
        size_t n = 0;
        for (unsigned char c : utf8) {
            if ((c & 0xC0) != 0x80) ++n;
        }
        return n;
    }

    std::string Trimmed(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    std::string Shorten(const std::string& text, size_t max) {
        std::string line = text;
        std::replace(line.begin(), line.end(), '\r', ' ');
        std::replace(line.begin(), line.end(), '\n', ' ');
        line = Trimmed(line);
        if (Length(line) <= max || max < 2) return line;
        // cut on a letter, not inside one
        size_t letters = 0, i = 0;
        for (; i < line.size(); ++i) {
            if ((static_cast<unsigned char>(line[i]) & 0xC0) != 0x80) {
                if (letters == max - 1) break;
                ++letters;
            }
        }
        return Trimmed(line.substr(0, i)) + u8"…";
    }
}
