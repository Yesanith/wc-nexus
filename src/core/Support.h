#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "Api.h"
#include "Models.h"

namespace wc {

    /// what a failure is called to the player. the codes are the hints of the backend, the same ones the app words
    namespace ErrorText {
        std::string For(const ApiError& e);
        std::string ForUnknown();
        std::string ForLink(const std::string& error);

        /// every English text the codes are worded with, for the tests
        std::vector<std::string> AllTexts();
    }

    /// which events the player is to be told of, and when: an event they are in (or made) that begins within [minutes], told once
    class ReminderPlanner {
    public:
        /// the events that are to be announced now. an event is announced once, and one that has begun long ago is not (a player who opens the
        /// game ten minutes after the start is not told that it is starting)
        std::vector<EventInfo> Due(const std::vector<EventInfo>& events, Moment now, int minutes);

        /// the ids are not kept with their times: an event is told once per run of the game. the set is cleared when it grows large
        void Forget();

        /// "Raid night starts in 5 minutes", "Raid night is starting now"
        static std::string Message(const EventInfo& e, Moment now);

    private:
        std::set<std::string> m_told;
    };

    /// keeps the token of the addon in a file of its own folder: not a setting, so it is not shown with the others
    class TokenStore {
    public:
        explicit TokenStore(std::filesystem::path directory);

        std::string Load() const;

        /// written whole to a side file and moved over, so a crash leaves the old token and not half of a new one. false when it could not be
        bool Save(const std::string& token) const;
        void Clear() const;

        /// tcm_ and 64 small hexadecimal characters
        static bool IsToken(const std::string& s);

    private:
        std::filesystem::path m_path;
    };

    /// what the player chose and the addon has to remember
    struct Settings {
        std::string language = "auto";  // "auto" follows the language of Nexus, otherwise a code of Loc::Codes
        bool reminders = true;
        int reminderMinutes = 10;       // 1 to 60
        int refreshMinutes = 5;         // 1 to 30
        int windowSize = 100;           // 60 to 200, in percent
        std::string defaultAccountId;   // the game account that signs up to public events; empty is automatic

        /// the values held to their ranges
        void Clamp();

        static Settings Load(const std::filesystem::path& directory);
        bool Save(const std::filesystem::path& directory) const;
    };

    /// the address of a public event on the website, which opens the app at it
    std::string PublicEventLink(const std::string& id);
}
