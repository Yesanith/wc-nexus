// the host of the addon as the tests of the window have it: no Nexus, a folder of its own, English, and a Thief
#include <filesystem>
#include <string>
#include <vector>

#include "addon/Host.h"

namespace wc::Host {
    std::vector<std::string> g_alerts;
    std::filesystem::path g_folder;

    void Attach(AddonAPI_t*) {}
    void Detach() {}
    AddonAPI_t* Api() { return nullptr; }
    std::filesystem::path Folder() { return g_folder; }
    void Info(const std::string&) {}
    void Warn(const std::string&) {}
    void Alert(const std::string& text) { g_alerts.push_back(text); }
    std::string HostLanguage() { return "en"; }
    void RegisterTexts() {}
    void FrameForFonts() {}
    ImFont* BodyFont() { return nullptr; }
    ImFont* StrongFont() { return nullptr; }
    ImFont* TitleFont() { return nullptr; }
    std::string Profession() { return "Thief"; }
}
