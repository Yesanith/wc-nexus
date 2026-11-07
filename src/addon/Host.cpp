#include "Host.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "imgui.h"
#include "nexus/Nexus.h"

#include "core/Models.h"
#include "core/Rules.h"
#include "core/Text.h"

namespace wc::Host {

    namespace {
        AddonAPI_t* g_api = nullptr;
        ImFont* g_body = nullptr;
        ImFont* g_strong = nullptr;
        ImFont* g_title = nullptr;
        int g_framesToFonts = -1;
        bool g_fontsAsked = false;

        // the identifiers the addon's texts and fonts are known by in Nexus
        constexpr const char* kLanguageProbe = "WC_LANGUAGE";
        constexpr const char* kBody = "WC_FONT_BODY";
        constexpr const char* kBodyMerge = "WC_FONT_BODY_CJK";
        constexpr const char* kStrong = "WC_FONT_STRONG";
        constexpr const char* kStrongMerge = "WC_FONT_STRONG_CJK";
        constexpr const char* kTitle = "WC_FONT_TITLE";
        constexpr const char* kTitleMerge = "WC_FONT_TITLE_CJK";

        /// the shared memory the game writes for Mumble: the identity of the character is a json text in it
        struct LinkedMem {
            uint32_t uiVersion;
            uint32_t uiTick;
            float fAvatarPosition[3];
            float fAvatarFront[3];
            float fAvatarTop[3];
            wchar_t name[256];
            float fCameraPosition[3];
            float fCameraFront[3];
            float fCameraTop[3];
            wchar_t identity[256];
            uint32_t context_len;
            unsigned char context[256];
            wchar_t description[2048];
        };

        std::string Narrow(const wchar_t* w, size_t max) {
            size_t n = 0;
            while (n < max && w[n]) ++n;
            if (n == 0) return {};
            int size = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
            std::string s(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(n), s.data(), size, nullptr, nullptr);
            return s;
        }

        std::string WindowsFont(const char* file) {
            char dir[MAX_PATH] = {};
            UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
            std::string path = (n > 0 ? std::string(dir, n) : std::string("C:\\Windows")) + "\\Fonts\\" + file;
            return GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES ? "" : path;
        }

        void OnBody(const char*, void* font) { g_body = static_cast<ImFont*>(font); }
        void OnStrong(const char*, void* font) { g_strong = static_cast<ImFont*>(font); }
        void OnTitle(const char*, void* font) { g_title = static_cast<ImFont*>(font); }
        void OnMerged(const char*, void*) {}

        /// one font of Windows for the letters of Europe and Cyrillic, with the Chinese and Japanese ones of another merged into it. a font that is
        /// not on the machine is left out: the default font of Nexus is used instead
        void AddFont(const char* id, const char* mergeId, const char* file, float size, FONTS_RECEIVECALLBACK callback) {
            std::string path = WindowsFont(file);
            if (path.empty()) return;
            g_api->Fonts_AddFromFile(id, size, path.c_str(), callback, nullptr);
            std::string cjk = WindowsFont("msyh.ttc");
            if (cjk.empty()) cjk = WindowsFont("YuGothM.ttc");
            if (cjk.empty()) return;
            // Nexus builds the fonts in the order they were given: a font in merge mode goes into the one before it
            static ImFontConfig merge = [] {
                ImFontConfig c;
                c.MergeMode = true;
                c.OversampleH = 1;
                c.OversampleV = 1;
                return c;
            }();
            g_api->Fonts_AddFromFile(mergeId, size, cjk.c_str(), OnMerged, &merge);
        }

        /// a text of every letter of a script, so that letters the addon's own words do not have are in the fonts too: what players write
        std::string Range(unsigned from, unsigned to) {
            std::string s;
            for (unsigned c = from; c <= to; ++c) {
                if (c < 0x800) {
                    s += static_cast<char>(0xC0 | (c >> 6));
                    s += static_cast<char>(0x80 | (c & 0x3F));
                } else {
                    s += static_cast<char>(0xE0 | (c >> 12));
                    s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                    s += static_cast<char>(0x80 | (c & 0x3F));
                }
            }
            return s;
        }
    }

    void Attach(AddonAPI_t* api) { g_api = api; }

    void Detach() {
        if (g_api && g_fontsAsked) {
            g_api->Fonts_Release(kBody, OnBody);
            g_api->Fonts_Release(kStrong, OnStrong);
            g_api->Fonts_Release(kTitle, OnTitle);
            g_api->Fonts_Release(kBodyMerge, OnMerged);
            g_api->Fonts_Release(kStrongMerge, OnMerged);
            g_api->Fonts_Release(kTitleMerge, OnMerged);
        }
        g_body = g_strong = g_title = nullptr;
        g_api = nullptr;
        g_fontsAsked = false;
        g_framesToFonts = -1;
    }

    AddonAPI_t* Api() { return g_api; }

    std::filesystem::path Folder() {
        std::filesystem::path dir;
        if (g_api && g_api->Paths_GetAddonDirectory) {
            const char* p = g_api->Paths_GetAddonDirectory("WondrousCompass");
            if (p) dir = std::filesystem::u8path(p);
        }
        if (dir.empty()) dir = std::filesystem::current_path() / "addons" / "WondrousCompass";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    void Info(const std::string& text) {
        if (g_api && g_api->Log) g_api->Log(LOGL_INFO, "Wondrous Compass", text.c_str());
    }

    void Warn(const std::string& text) {
        if (g_api && g_api->Log) g_api->Log(LOGL_WARNING, "Wondrous Compass", text.c_str());
    }

    void Alert(const std::string& text) {
        if (g_api && g_api->GUI_SendAlert) g_api->GUI_SendAlert(text.c_str());
    }

    std::string HostLanguage() {
        if (!g_api || !g_api->Localization_Translate) return "en";
        const char* code = g_api->Localization_Translate(kLanguageProbe);
        // before Nexus has taken in the probe, it gives back the identifier itself
        if (!code || std::string(code) == kLanguageProbe) return "en";
        return Loc::FromHost(code);
    }

    void RegisterTexts() {
        if (!g_api || !g_api->Localization_Set) return;
        // the probe answers each language of Nexus with its own code, which is how the addon knows the language Nexus speaks
        for (const char* code : {"en", "de", "fr", "es", "cn", "br", "cz", "it", "pl", "ru", "kr"}) {
            g_api->Localization_Set(kLanguageProbe, code, code);
            // the key that opens the window, as the keybinds of Nexus name it
            g_api->Localization_Set("KB_WONDROUS_COMPASS_TOGGLE", code, "Wondrous Compass");
        }
        int n = 0;
        for (const auto& code : Loc::Codes()) {
            for (const auto& e : Loc::TableOf(code)) {
                std::string id = "WC_TEXT_" + std::to_string(n++);
                g_api->Localization_Set(id.c_str(), "en", e.text);
            }
        }
        // Cyrillic, Greek and the kana of Japanese whole, for names and notes that players write
        g_api->Localization_Set("WC_GLYPHS_CYRILLIC", "en", Range(0x0400, 0x04FF).c_str());
        g_api->Localization_Set("WC_GLYPHS_GREEK", "en", Range(0x0370, 0x03FF).c_str());
        g_api->Localization_Set("WC_GLYPHS_KANA", "en", Range(0x3040, 0x30FF).c_str());
        g_api->Localization_Set("WC_GLYPHS_PUNCTUATION", "en", (Range(0x2010, 0x2027) + Range(0x3000, 0x303F) + Range(0xFF01, 0xFF5E)).c_str());
        g_framesToFonts = 10;
    }

    void FrameForFonts() {
        if (g_fontsAsked || g_framesToFonts < 0 || !g_api || !g_api->Fonts_AddFromFile) return;
        if (--g_framesToFonts > 0) return;
        g_fontsAsked = true;
        AddFont(kBody, kBodyMerge, "segoeui.ttf", 16.0f, OnBody);
        AddFont(kStrong, kStrongMerge, "segoeuib.ttf", 16.0f, OnStrong);
        AddFont(kTitle, kTitleMerge, "segoeuib.ttf", 21.0f, OnTitle);
    }

    ImFont* BodyFont() { return g_body; }
    ImFont* StrongFont() { return g_strong ? g_strong : g_body; }
    ImFont* TitleFont() { return g_title ? g_title : StrongFont(); }

    std::string Profession() {
        if (!g_api || !g_api->DataLink_Get) return "";
        auto* mem = static_cast<LinkedMem*>(g_api->DataLink_Get(DL_MUMBLE_LINK));
        if (!mem || mem->uiTick == 0) return "";
        std::string identity = Narrow(mem->identity, 256);
        Json j = Json::parse(identity, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return "";
        return EventRules::ProfessionOfId(Int(j, "profession").value_or(0));
    }
}
