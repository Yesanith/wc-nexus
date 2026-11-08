#include "Time.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "Text.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wc {

    namespace {
        std::tm LocalTm(Moment at) {
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &at);
#else
            localtime_r(&at, &tm);
#endif
            return tm;
        }

        std::tm UtcTm(Moment at) {
            std::tm tm{};
#ifdef _WIN32
            gmtime_s(&tm, &at);
#else
            gmtime_r(&at, &tm);
#endif
            return tm;
        }

        // days from 1970-01-01 to a date of the proleptic Gregorian calendar
        long long DaysFromCivil(long long y, unsigned m, unsigned d) {
            y -= m <= 2;
            const long long era = (y >= 0 ? y : y - 399) / 400;
            const unsigned yoe = static_cast<unsigned>(y - era * 400);
            const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
            const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + static_cast<long long>(doe) - 719468;
        }

        std::string Lower(std::string s) {
            // only the letters of ASCII are made small: the words below are compared as they are written
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        std::string Trim(const std::string& s) {
            size_t a = 0, b = s.size();
            while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
            while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
            return s.substr(a, b - a);
        }

        bool AllDigits(const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
        }

        /// "20", "20:00", "9:05": hours and minutes, or nothing
        std::optional<std::pair<int, int>> ClockOf(const std::string& s) {
            auto colon = s.find(':');
            std::string h = colon == std::string::npos ? s : s.substr(0, colon);
            std::string m = colon == std::string::npos ? "" : s.substr(colon + 1);
            if (!AllDigits(h) || h.size() > 2) return std::nullopt;
            if (colon != std::string::npos && (m.size() != 2 || !AllDigits(m))) return std::nullopt;
            int hour = std::stoi(h), minute = m.empty() ? 0 : std::stoi(m);
            if (hour > 23 || minute > 59) return std::nullopt;
            return std::make_pair(hour, minute);
        }

        bool StartsWith(const std::string& s, const char* word) { return s.compare(0, std::strlen(word), word) == 0; }

        // the words for tomorrow and today, in English and in each language of the addon; letters outside ASCII are compared as written.
        // a word is matched by its start, so the whole of aujourd'hui (with either apostrophe) comes before the start of it
        const char* kTomorrow[] = {"tomorrow", "morgen", u8"mañana", "demain", u8"amanhã", "yarin", u8"yarın", "zavtra", u8"завтра", u8"明天", u8"明日"};
        const char* kToday[] = {"today", "heute", "hoy", "aujourd'hui", u8"aujourd’hui", "aujourd", "hoje", "bugun", u8"bugün", "segodnya", u8"сегодня",
                                u8"今天", u8"今日"};

        std::string Two(int n) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%02d", n);
            return buf;
        }
    }

    namespace Clock {
        LocalTime Local(Moment at) {
            std::tm tm = LocalTm(at);
            return {tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday};
        }

        Moment FromLocal(int year, int month, int day, int hour, int minute, int second) {
            std::tm tm{};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;
            tm.tm_isdst = -1;
            return std::mktime(&tm);
        }

        Moment StartOfDay(Moment at) {
            LocalTime l = Local(at);
            return FromLocal(l.year, l.month, l.day, 0, 0, 0);
        }

        std::optional<Moment> ParseIso(const std::string& text) {
            int y, mo, d, h = 0, mi = 0, s = 0;
            const char* p = text.c_str();
            int n = 0;
            if (std::sscanf(p, "%4d-%2d-%2d%n", &y, &mo, &d, &n) != 3) return std::nullopt;
            p += n;
            if (*p == 'T' || *p == 't' || *p == ' ') {
                ++p;
                n = 0;
                if (std::sscanf(p, "%2d:%2d%n", &h, &mi, &n) != 2) return std::nullopt;
                p += n;
                if (*p == ':') {
                    ++p;
                    n = 0;
                    if (std::sscanf(p, "%2d%n", &s, &n) != 1) return std::nullopt;
                    p += n;
                }
                if (*p == '.') {
                    ++p;
                    while (*p >= '0' && *p <= '9') ++p;
                }
            }
            long offset = 0;
            if (*p == 'Z' || *p == 'z') {
                ++p;
            } else if (*p == '+' || *p == '-') {
                int sign = *p == '-' ? -1 : 1;
                ++p;
                int oh = 0, om = 0;
                if (!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9')) return std::nullopt;
                oh = (p[0] - '0') * 10 + (p[1] - '0');
                p += 2;
                if (*p == ':') ++p;
                if (p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9') {
                    om = (p[0] - '0') * 10 + (p[1] - '0');
                    p += 2;
                }
                offset = sign * (oh * 3600L + om * 60L);
            }
            if (*p != '\0') return std::nullopt;
            if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return std::nullopt;
            long long days = DaysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d));
            return static_cast<Moment>(days * 86400LL + h * 3600LL + mi * 60LL + s - offset);
        }

        std::string ToIsoUtc(Moment at) {
            std::tm tm = UtcTm(at);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            return buf;
        }

        std::string HourMinute(Moment at) {
            LocalTime l = Local(at);
            return Two(l.hour) + ":" + Two(l.minute);
        }

        std::string Typed(Moment at) {
            LocalTime l = Local(at);
            return std::to_string(l.year) + "-" + Two(l.month) + "-" + Two(l.day) + " " + Two(l.hour) + ":" + Two(l.minute);
        }
    }

    namespace TimeInput {
        std::optional<Moment> Parse(const std::string& text, Moment now) {
            std::string s = Lower(Trim(text));
            if (s.empty()) return std::nullopt;
            LocalTime today = Clock::Local(now);

            // "+2d", "+1d 20:00": days from today, at the time now or at the one given
            if (s[0] == '+') {
                size_t i = 1;
                while (i < s.size() && i < 4 && s[i] >= '0' && s[i] <= '9') ++i;
                if (i > 1 && i < s.size() && s[i] == 'd') {
                    int days = std::stoi(s.substr(1, i - 1));
                    std::string rest = Trim(s.substr(i + 1));
                    if (rest.empty()) return Clock::FromLocal(today.year, today.month, today.day + days, today.hour, today.minute, 0);
                    auto c = ClockOf(rest);
                    if (!c) return std::nullopt;
                    return Clock::FromLocal(today.year, today.month, today.day + days, c->first, c->second, 0);
                }
                return std::nullopt;
            }

            for (const char* word : kTomorrow) {
                if (!StartsWith(s, word)) continue;
                auto c = ClockOf(Trim(s.substr(std::strlen(word))));
                if (!c) return std::nullopt;
                return Clock::FromLocal(today.year, today.month, today.day + 1, c->first, c->second, 0);
            }
            for (const char* word : kToday) {
                if (!StartsWith(s, word)) continue;
                auto c = ClockOf(Trim(s.substr(std::strlen(word))));
                if (!c) return std::nullopt;
                return Clock::FromLocal(today.year, today.month, today.day, c->first, c->second, 0);
            }

            if (auto c = ClockOf(s)) {
                Moment at = Clock::FromLocal(today.year, today.month, today.day, c->first, c->second, 0);
                return at <= now ? Clock::FromLocal(today.year, today.month, today.day + 1, c->first, c->second, 0) : at;
            }

            // a date then a clock; a date without a year is this year, or the next when it has gone by
            auto space = s.find(' ');
            if (space == std::string::npos) return std::nullopt;
            std::string date = s.substr(0, space);
            auto c = ClockOf(Trim(s.substr(space + 1)));
            if (!c) return std::nullopt;
            int y, m, d;
            char tail;
            if (date.size() == 10 && std::sscanf(date.c_str(), "%4d-%2d-%2d%c", &y, &m, &d, &tail) == 3) {
                if (m < 1 || m > 12 || d < 1 || d > 31) return std::nullopt;
                return Clock::FromLocal(y, m, d, c->first, c->second, 0);
            }
            if (std::sscanf(date.c_str(), "%2d-%2d%c", &m, &d, &tail) == 2 && date.find('-') != std::string::npos) {
                if (m < 1 || m > 12 || d < 1 || d > 31) return std::nullopt;
                Moment at = Clock::FromLocal(today.year, m, d, c->first, c->second, 0);
                return at < now ? Clock::FromLocal(today.year + 1, m, d, c->first, c->second, 0) : at;
            }
            return std::nullopt;
        }

        std::string Describe(Moment at, Moment now) {
            std::string clock = Clock::HourMinute(at);
            Moment day = Clock::StartOfDay(at);
            Moment today = Clock::StartOfDay(now);
            LocalTime t = Clock::Local(now);
            Moment tomorrow = Clock::FromLocal(t.year, t.month, t.day + 1, 0, 0, 0);
            if (day == today) return Loc::T("Today {0}", {clock});
            if (day == tomorrow) return Loc::T("Tomorrow {0}", {clock});
            LocalTime l = Clock::Local(at);
            return std::string(Loc::Weekday(l.weekday)) + " " + std::to_string(l.day) + " " + Loc::Month(l.month - 1) + " " + clock;
        }
    }

    WeeklySlot WeeklySlot::Of(Moment start, const std::string& windowsZoneId) {
        std::string zone = TimeZones::ToIana(windowsZoneId);
        WeeklySlot slot;
        if (zone.empty()) {
            std::tm tm = UtcTm(start);
            slot.weekday = tm.tm_wday == 0 ? 7 : tm.tm_wday;
            slot.timeOfDay = Two(tm.tm_hour) + ":" + Two(tm.tm_min);
            slot.zone = "UTC";
            return slot;
        }
        LocalTime l = Clock::Local(start);
        slot.weekday = l.weekday == 0 ? 7 : l.weekday;
        slot.timeOfDay = Two(l.hour) + ":" + Two(l.minute);
        slot.zone = zone;
        return slot;
    }

    namespace TimeZones {
        std::string ToIana(const std::string& windowsId) {
            static const std::map<std::string, std::string> map = {
                {"utc", "UTC"},
                {"gmt standard time", "Europe/London"},
                {"greenwich standard time", "Atlantic/Reykjavik"},
                {"w. europe standard time", "Europe/Berlin"},
                {"central europe standard time", "Europe/Budapest"},
                {"central european standard time", "Europe/Warsaw"},
                {"romance standard time", "Europe/Paris"},
                {"e. europe standard time", "Europe/Chisinau"},
                {"gtb standard time", "Europe/Bucharest"},
                {"fle standard time", "Europe/Kiev"},
                {"turkey standard time", "Europe/Istanbul"},
                {"russian standard time", "Europe/Moscow"},
                {"belarus standard time", "Europe/Minsk"},
                {"israel standard time", "Asia/Jerusalem"},
                {"arab standard time", "Asia/Riyadh"},
                {"arabian standard time", "Asia/Dubai"},
                {"egypt standard time", "Africa/Cairo"},
                {"south africa standard time", "Africa/Johannesburg"},
                {"india standard time", "Asia/Kolkata"},
                {"se asia standard time", "Asia/Bangkok"},
                {"china standard time", "Asia/Shanghai"},
                {"singapore standard time", "Asia/Singapore"},
                {"taipei standard time", "Asia/Taipei"},
                {"tokyo standard time", "Asia/Tokyo"},
                {"korea standard time", "Asia/Seoul"},
                {"aus eastern standard time", "Australia/Sydney"},
                {"e. australia standard time", "Australia/Brisbane"},
                {"cen. australia standard time", "Australia/Adelaide"},
                {"w. australia standard time", "Australia/Perth"},
                {"new zealand standard time", "Pacific/Auckland"},
                {"eastern standard time", "America/New_York"},
                {"central standard time", "America/Chicago"},
                {"mountain standard time", "America/Denver"},
                {"us mountain standard time", "America/Phoenix"},
                {"pacific standard time", "America/Los_Angeles"},
                {"alaskan standard time", "America/Anchorage"},
                {"hawaiian standard time", "Pacific/Honolulu"},
                {"atlantic standard time", "America/Halifax"},
                {"canada central standard time", "America/Regina"},
                {"e. south america standard time", "America/Sao_Paulo"},
                {"argentina standard time", "America/Argentina/Buenos_Aires"},
                {"sa pacific standard time", "America/Bogota"},
                {"central america standard time", "America/Guatemala"},
                // Windows names the zones of Mexico by the zone they are like, with (Mexico) after it
                {"central standard time (mexico)", "America/Mexico_City"},
                {"mountain standard time (mexico)", "America/Mazatlan"},
                {"pacific standard time (mexico)", "America/Tijuana"},
            };
            auto it = map.find(Lower(windowsId));
            return it == map.end() ? "" : it->second;
        }

        std::string LocalWindowsId() {
#ifdef _WIN32
            DYNAMIC_TIME_ZONE_INFORMATION info{};
            if (GetDynamicTimeZoneInformation(&info) == TIME_ZONE_ID_INVALID) return "";
            char buf[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, info.TimeZoneKeyName, -1, buf, sizeof(buf), nullptr, nullptr);
            return buf;
#else
            return "";
#endif
        }
    }
}
