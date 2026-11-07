#include "Text.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace wc {

    namespace {
        struct Pair {
            const char* english;
            const char* text;
        };

        struct Table {
            const char* code;
            const Pair* pairs;
            size_t count;
        };

#include "lang/Tables.inc"

        std::string g_language = "en";
        std::unordered_map<std::string, const char*> g_table;

        const Table* Find(const std::string& code) {
            for (const auto& t : kTables) {
                if (code == t.code) return &t;
            }
            return nullptr;
        }

        // short names of the days and the months, as a list shows a date
        const char* kDays[9][7] = {
            {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
            {u8"So", u8"Mo", u8"Di", u8"Mi", u8"Do", u8"Fr", u8"Sa"},
            {u8"dom", u8"lun", u8"mar", u8"mié", u8"jue", u8"vie", u8"sáb"},
            {u8"dim.", u8"lun.", u8"mar.", u8"mer.", u8"jeu.", u8"ven.", u8"sam."},
            {u8"dom", u8"seg", u8"ter", u8"qua", u8"qui", u8"sex", u8"sáb"},
            {u8"Paz", u8"Pzt", u8"Sal", u8"Çar", u8"Per", u8"Cum", u8"Cmt"},
            {u8"вс", u8"пн", u8"вт", u8"ср", u8"чт", u8"пт", u8"сб"},
            {u8"日", u8"月", u8"火", u8"水", u8"木", u8"金", u8"土"},
            {u8"周日", u8"周一", u8"周二", u8"周三", u8"周四", u8"周五", u8"周六"},
        };
        const char* kMonths[9][12] = {
            {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
            {u8"Jan.", u8"Feb.", u8"März", u8"Apr.", u8"Mai", u8"Juni", u8"Juli", u8"Aug.", u8"Sept.", u8"Okt.", u8"Nov.", u8"Dez."},
            {u8"ene", u8"feb", u8"mar", u8"abr", u8"may", u8"jun", u8"jul", u8"ago", u8"sept", u8"oct", u8"nov", u8"dic"},
            {u8"janv.", u8"févr.", u8"mars", u8"avr.", u8"mai", u8"juin", u8"juil.", u8"août", u8"sept.", u8"oct.", u8"nov.", u8"déc."},
            {u8"jan", u8"fev", u8"mar", u8"abr", u8"mai", u8"jun", u8"jul", u8"ago", u8"set", u8"out", u8"nov", u8"dez"},
            {u8"Oca", u8"Şub", u8"Mar", u8"Nis", u8"May", u8"Haz", u8"Tem", u8"Ağu", u8"Eyl", u8"Eki", u8"Kas", u8"Ara"},
            {u8"янв.", u8"февр.", u8"мар.", u8"апр.", u8"мая", u8"июн.", u8"июл.", u8"авг.", u8"сент.", u8"окт.", u8"нояб.", u8"дек."},
            {u8"1月", u8"2月", u8"3月", u8"4月", u8"5月", u8"6月", u8"7月", u8"8月", u8"9月", u8"10月", u8"11月", u8"12月"},
            {u8"1月", u8"2月", u8"3月", u8"4月", u8"5月", u8"6月", u8"7月", u8"8月", u8"9月", u8"10月", u8"11月", u8"12月"},
        };

        int IndexOf(const std::string& code) {
            const auto& codes = Loc::Codes();
            auto it = std::find(codes.begin(), codes.end(), code);
            return it == codes.end() ? 0 : static_cast<int>(it - codes.begin());
        }
    }

    namespace Loc {
        const std::vector<std::string>& Codes() {
            static const std::vector<std::string> codes = {"en", "de", "es", "fr", "pt", "tr", "ru", "ja", "zh"};
            return codes;
        }

        void Use(const std::string& code) {
            const auto& codes = Codes();
            g_language = std::find(codes.begin(), codes.end(), code) == codes.end() ? "en" : code;
            g_table.clear();
            if (const Table* t = Find(g_language)) {
                for (size_t i = 0; i < t->count; ++i) g_table[t->pairs[i].english] = t->pairs[i].text;
            }
        }

        const std::string& Language() { return g_language; }

        const char* T(const char* english) {
            if (!english) return "";
            auto it = g_table.find(english);
            return it == g_table.end() ? english : it->second;
        }

        std::string T(const char* english, std::initializer_list<std::string> args) { return Format(T(english), args); }

        std::vector<Entry> TableOf(const std::string& code) {
            std::vector<Entry> list;
            if (const Table* t = Find(code)) {
                for (size_t i = 0; i < t->count; ++i) list.push_back({t->pairs[i].english, t->pairs[i].text});
            }
            return list;
        }

        const char* NativeName(const std::string& code) {
            if (code == "de") return u8"Deutsch";
            if (code == "es") return u8"Español";
            if (code == "fr") return u8"Français";
            if (code == "pt") return u8"Português";
            if (code == "tr") return u8"Türkçe";
            if (code == "ru") return u8"Русский";
            if (code == "ja") return u8"日本語";
            if (code == "zh") return u8"中文";
            return "English";
        }

        std::string FromHost(const std::string& hostCode) {
            if (hostCode == "de" || hostCode == "es" || hostCode == "fr" || hostCode == "ru") return hostCode;
            if (hostCode == "cn") return "zh";
            if (hostCode == "br") return "pt";
            return "en";
        }

        const char* Weekday(int day) { return kDays[IndexOf(g_language)][((day % 7) + 7) % 7]; }
        const char* Month(int month) { return kMonths[IndexOf(g_language)][((month % 12) + 12) % 12]; }
    }

    std::string Format(const std::string& pattern, std::initializer_list<std::string> args) {
        std::string out;
        out.reserve(pattern.size() + 16);
        for (size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i] == '{') {
                size_t close = pattern.find('}', i);
                if (close != std::string::npos && close > i + 1) {
                    bool digits = true;
                    for (size_t j = i + 1; j < close; ++j) digits = digits && pattern[j] >= '0' && pattern[j] <= '9';
                    if (digits) {
                        size_t n = std::stoul(pattern.substr(i + 1, close - i - 1));
                        if (n < args.size()) out += *(args.begin() + n);
                        i = close;
                        continue;
                    }
                }
            }
            out += pattern[i];
        }
        return out;
    }

    std::vector<std::string> Placeholders(const std::string& text) {
        std::vector<std::string> found;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] != '{') continue;
            size_t close = text.find('}', i);
            if (close == std::string::npos || close == i + 1) continue;
            bool digits = true;
            for (size_t j = i + 1; j < close; ++j) digits = digits && text[j] >= '0' && text[j] <= '9';
            if (digits) found.push_back(text.substr(i, close - i + 1));
        }
        std::sort(found.begin(), found.end());
        return found;
    }
}
