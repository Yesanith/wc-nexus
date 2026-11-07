#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#include "core/Rules.h"
#include "core/Support.h"
#include "core/Text.h"
#include "core/Time.h"
#include "doctest/doctest.h"

using namespace wc;

namespace {
    std::string Read(const std::filesystem::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string Unescape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char n = s[++i];
                out += n == 'n' ? '\n' : n == 't' ? '\t' : n;
            } else {
                out += s[i];
            }
        }
        return out;
    }

    /// the English texts the code asks for: every string in the first argument of each T(...), the texts of the refusals, and the words the
    /// code looks up by a name that is not in a call
    std::set<std::string> UsedKeys() {
        std::set<std::string> keys;
        std::filesystem::path root = std::filesystem::path(WC_SOURCE_DIR) / "src";
        const std::regex literal("(?:u8)?\"((?:[^\"\\\\]|\\\\.)*)\"");
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".h") continue;
            if (entry.path().filename() == "Text.cpp") continue;
            std::string text = Read(entry.path());
            size_t at = 0;
            while ((at = text.find("T(", at)) != std::string::npos) {
                // a call of T, not the end of another name
                bool call = at == 0 || !(std::isalnum(static_cast<unsigned char>(text[at - 1])) || text[at - 1] == '_');
                size_t i = at + 2;
                at = i;
                if (!call) continue;
                int depth = 0;
                bool inString = false;
                size_t j = i;
                for (; j < text.size(); ++j) {
                    char c = text[j];
                    if (inString) {
                        if (c == '\\') ++j;
                        else if (c == '"') inString = false;
                    } else if (c == '"') inString = true;
                    else if (c == '(') ++depth;
                    else if (c == ')') {
                        if (depth == 0) break;
                        --depth;
                    } else if ((c == ',' || c == '{') && depth == 0) break;
                }
                std::string arg = text.substr(i, j - i);
                for (std::sregex_iterator m(arg.begin(), arg.end(), literal), end; m != end; ++m) {
                    std::string key = Unescape((*m)[1].str());
                    if (std::any_of(key.begin(), key.end(), [](char c) { return std::isalpha(static_cast<unsigned char>(c)); })) keys.insert(key);
                }
            }
        }
        for (const auto& t : ErrorText::AllTexts()) keys.insert(t);
        for (const char* extra : {"going", "maybe", "waiting", "declined", "leader", "officer", "member", "No roles", "Squad of 10", "Party of 5", "none"}) {
            keys.insert(extra);
        }
        return keys;
    }

    int Count(const std::string& s, char c) { return static_cast<int>(std::count(s.begin(), s.end(), c)); }

    const std::vector<std::string> Translated = {"de", "es", "fr", "pt", "tr", "ru", "ja", "zh"};
}

TEST_SUITE("languages") {
    TEST_CASE("the code asks for some hundreds of texts") {
        auto keys = UsedKeys();
        CHECK(keys.size() > 250);
    }

    TEST_CASE("every text has a word in each language") {
        auto keys = UsedKeys();
        for (const auto& code : Translated) {
            std::set<std::string> have;
            for (const auto& e : Loc::TableOf(code)) have.insert(e.english);
            for (const auto& key : keys) {
                CAPTURE(code);
                CAPTURE(key);
                CHECK(have.count(key) == 1);
            }
        }
    }

    TEST_CASE("no language has a word that is not used") {
        auto keys = UsedKeys();
        for (const auto& code : Translated) {
            for (const auto& e : Loc::TableOf(code)) {
                CAPTURE(code);
                CAPTURE(e.english);
                CHECK(keys.count(e.english) == 1);
            }
        }
    }

    TEST_CASE("placeholders and line breaks agree") {
        for (const auto& code : Translated) {
            for (const auto& e : Loc::TableOf(code)) {
                CAPTURE(code);
                CAPTURE(e.english);
                CHECK(Placeholders(e.english) == Placeholders(e.text));
                CHECK(Count(e.english, '\n') == Count(e.text, '\n'));
                CHECK(std::string(e.text).size() > 0);
            }
        }
    }

    TEST_CASE("english is the key itself, and an unknown language is english") {
        Loc::Use("en");
        CHECK(std::string(Loc::T("Sign up")) == "Sign up");
        Loc::Use("xx");
        CHECK(Loc::Language() == "en");
        Loc::Use("tr");
        CHECK(std::string(Loc::T("a text with no word")) == "a text with no word");
        Loc::Use("en");
    }

    TEST_CASE("the language of the host becomes a language of the addon") {
        CHECK(Loc::FromHost("de") == "de");
        CHECK(Loc::FromHost("cn") == "zh");
        CHECK(Loc::FromHost("br") == "pt");
        CHECK(Loc::FromHost("ru") == "ru");
        CHECK(Loc::FromHost("kr") == "en");
        CHECK(Loc::FromHost("") == "en");
    }

    TEST_CASE("labels and dates follow the language") {
        Moment now = Clock::FromLocal(2026, 10, 9, 18, 30);
        Loc::Use("tr");
        CHECK(EventRules::RoleLabel("healer") != "Healer");
        CHECK(TimeInput::Describe(Clock::FromLocal(2026, 10, 12, 20, 0), now) == u8"Pzt 12 Eki 20:00");
        Loc::Use("de");
        CHECK(TimeInput::Describe(Clock::FromLocal(2026, 10, 9, 20, 0), now).find("20:00") != std::string::npos);
        Loc::Use("en");
    }

    TEST_CASE("a refusal is told in the language") {
        Loc::Use("tr");
        CHECK(ErrorText::For(ApiError("role_full", "", 400)) != "That role is full.");
        Loc::Use("en");
    }

    TEST_CASE("format fills the places in order") {
        CHECK(Format("{0} of {1}", {"a", "b"}) == "a of b");
        CHECK(Format("{1} then {0}", {"a", "b"}) == "b then a");
        CHECK(Format("{x} stays", {"a"}) == "{x} stays");
        CHECK(Placeholders("{1} and {0}") == std::vector<std::string>{"{0}", "{1}"});
    }
}
