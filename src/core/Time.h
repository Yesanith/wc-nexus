#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace wc {

    /// a moment is the seconds since 1970 in UTC, as std::time_t has it. what the player reads and types is in the local time of the machine
    using Moment = std::time_t;

    /// a moment as the local calendar has it
    struct LocalTime {
        int year = 1970, month = 1, day = 1;  // month 1 to 12
        int hour = 0, minute = 0, second = 0;
        int weekday = 4;                      // 0 Sunday
    };

    namespace Clock {
        LocalTime Local(Moment at);

        /// the moment of a local date and time; a day or a month out of range runs over into the next, as mktime does
        Moment FromLocal(int year, int month, int day, int hour, int minute, int second = 0);

        /// the start of the local day of a moment
        Moment StartOfDay(Moment at);

        /// what the server wrote: "2026-12-24T19:00:00+00:00", "2026-12-24T19:00:00.123Z", "2026-12-24 19:00:00+01". nothing when it is none
        std::optional<Moment> ParseIso(const std::string& text);

        /// what the server is sent: "2026-12-24T18:00:00Z"
        std::string ToIsoUtc(Moment at);

        /// "20:00" in the local time
        std::string HourMinute(Moment at);

        /// "2026-12-24 19:00" in the local time, as a time is typed
        std::string Typed(Moment at);
    }

    /// the time of an event as a player types it in the game, where a date picker is not worth the room:
    /// "20:00" (today, or tomorrow when that has passed), "+1d 20:00", "tomorrow 21:30", "2026-10-12 20:00", "10-12 20:00"
    namespace TimeInput {
        /// the moment meant, or nothing when the text is not a time
        std::optional<Moment> Parse(const std::string& text, Moment now);

        /// "Today 20:00", "Tomorrow 20:00", or the date, as the list shows an event
        std::string Describe(Moment at, Moment now);
    }

    /// when a weekly event comes back: the weekday (1 Monday to 7 Sunday), the time of day and the zone that time is in
    struct WeeklySlot {
        int weekday = 1;
        std::string timeOfDay;
        std::string zone;

        /// the slot of a first start. a zone the database cannot be told of is replaced by UTC, and the start with it
        static WeeklySlot Of(Moment start, const std::string& windowsZoneId);
    };

    namespace TimeZones {
        /// the IANA name of a Windows zone id ("W. Europe Standard Time" is Europe/Berlin), or empty when it is not known here
        std::string ToIana(const std::string& windowsId);

        /// the Windows id of the zone of the machine, or empty off Windows
        std::string LocalWindowsId();
    }
}
