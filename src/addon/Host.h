#pragma once

#include <filesystem>
#include <string>

struct AddonAPI_t;
struct ImFont;

namespace wc {

    /// what the addon asks of Nexus, in one place: where its files go, the log, a notice on the screen, the language of Nexus, the fonts that
    /// carry the letters of every language, and the character the player is on
    namespace Host {
        void Attach(AddonAPI_t* api);
        void Detach();
        AddonAPI_t* Api();

        /// the folder of the addon, made when it is not there: addons/WondrousCompass
        std::filesystem::path Folder();

        void Info(const std::string& text);
        void Warn(const std::string& text);

        /// a notice in the middle of the screen, as the game shows one
        void Alert(const std::string& text);

        /// the language Nexus speaks, as a language of the addon (en when Nexus speaks one the addon has no words for)
        std::string HostLanguage();

        /// tells Nexus the texts of every language, so that the fonts it builds have their letters, and asks for the fonts of the addon a
        /// little later: Nexus takes in texts once a frame, and a font built before then would lack them
        void RegisterTexts();
        void FrameForFonts();

        /// the fonts of the addon: the body, a strong one, and a big one for titles. nullptr until Nexus has built them, which is the
        /// default font of ImGui when pushed
        ImFont* BodyFont();
        ImFont* StrongFont();
        ImFont* TitleFont();

        /// the profession of the character the player is on, as the api names it ("Thief"), or empty when the game says nothing
        std::string Profession();
    }
}
