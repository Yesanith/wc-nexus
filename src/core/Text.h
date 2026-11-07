#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace wc {

    /// the words of the addon in the language of the player. a text is looked for by what it says in English, so the code reads as it does, and a
    /// text that a language has no translation for is shown in English
    namespace Loc {
        /// the languages there are words for, English first
        const std::vector<std::string>& Codes();

        /// the language the addon speaks from now on. one that is not known is English
        void Use(const std::string& code);
        const std::string& Language();

        /// the text in the language that is in use
        const char* T(const char* english);
        std::string T(const char* english, std::initializer_list<std::string> args);

        /// every English text of one language's table, and what it says, to tell the host the letters it needs
        struct Entry {
            const char* english;
            const char* text;
        };
        std::vector<Entry> TableOf(const std::string& code);

        /// the name of a language in that language: "Türkçe", "日本語"
        const char* NativeName(const std::string& code);

        /// the language of the host (Nexus names them en, de, fr, es, cn, br, ru, ...) as one of the addon. anything else is English
        std::string FromHost(const std::string& hostCode);

        /// the short name of a weekday (0 Sunday) and of a month (0 January) in the language that is in use
        const char* Weekday(int day);
        const char* Month(int month);
    }

    /// "{0} going" with its places filled: {0}, {1}... are the arguments in order
    std::string Format(const std::string& pattern, std::initializer_list<std::string> args);

    /// the placeholders of a text, in order, as written: {0}, {1}
    std::vector<std::string> Placeholders(const std::string& text);
}
