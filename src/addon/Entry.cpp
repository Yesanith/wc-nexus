#include <windows.h>

#include <cstring>
#include <memory>

#include "imgui.h"
#include "nexus/Nexus.h"

#include "Host.h"
#include "Resource.h"
#include "Ui.h"
#include "WinHttp.h"
#include "Worker.h"

#ifndef WC_VERSION_MAJOR
#define WC_VERSION_MAJOR 1
#define WC_VERSION_MINOR 0
#define WC_VERSION_PATCH 0
#endif

namespace {
    // the identifiers the addon is known by in Nexus
    constexpr const char* kToggleKey = "KB_WONDROUS_COMPASS_TOGGLE";
    constexpr const char* kShortcut = "QA_WONDROUS_COMPASS";
    constexpr const char* kIcon = "TEX_WONDROUS_COMPASS";
    constexpr const char* kIconHover = "TEX_WONDROUS_COMPASS_HOVER";

    HMODULE g_module = nullptr;
    AddonDefinition_t g_definition{};
    std::shared_ptr<wc::WinHttp> g_http;
    std::unique_ptr<wc::Worker> g_worker;
    std::unique_ptr<wc::ui::App> g_app;

    void Render() {
        wc::Host::FrameForFonts();
        if (g_app) g_app->Frame();
    }

    void Options() {
        if (g_app) g_app->Options();
    }

    void OnKey(const char* id, bool release) {
        if (release || !g_app || !id) return;
        if (std::strcmp(id, kToggleKey) == 0) g_app->Toggle();
    }

    void Load(AddonAPI_t* api) {
        wc::Host::Attach(api);
        // the addon draws with the ImGui of Nexus: its context and its memory
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
        ImGui::SetAllocatorFunctions(reinterpret_cast<void* (*)(size_t, void*)>(api->ImguiMalloc), reinterpret_cast<void (*)(void*, void*)>(api->ImguiFree));

        wc::Host::RegisterTexts();
        std::filesystem::path folder = wc::Host::Folder();
        wc::TokenStore store(folder);

        g_http = std::make_shared<wc::WinHttp>();
        auto codex = std::make_shared<wc::CodexApi>(g_http, wc::CodexApi::PublishableKey, store.Load());
        g_worker = std::make_unique<wc::Worker>([]() {
            if (g_http) g_http->Abort();
        });
        g_worker->Start();
        g_app = std::make_unique<wc::ui::App>(codex, *g_worker, store, folder);

        api->Textures_GetOrCreateFromResource(kIcon, IDR_QUICK_ACCESS, g_module);
        api->Textures_GetOrCreateFromResource(kIconHover, IDR_QUICK_ACCESS_HOVER, g_module);
        api->InputBinds_RegisterWithString(kToggleKey, OnKey, "(null)");
        api->QuickAccess_Add(kShortcut, kIcon, kIconHover, kToggleKey, "Wondrous Compass");
        api->GUI_Register(RT_Render, Render);
        api->GUI_Register(RT_OptionsRender, Options);
        api->GUI_RegisterCloseOnEscape(wc::ui::kWindowName, g_app->VisibleFlag());
        wc::Host::Info("loaded");
    }

    void Unload() {
        AddonAPI_t* api = wc::Host::Api();
        if (api) {
            api->GUI_DeregisterCloseOnEscape(wc::ui::kWindowName);
            api->GUI_Deregister(Options);
            api->GUI_Deregister(Render);
            api->QuickAccess_Remove(kShortcut);
            api->InputBinds_Deregister(kToggleKey);
        }
        // the call under way is cut and nothing more is run, before what it would have run is gone
        if (g_worker) g_worker->Stop();
        g_app.reset();
        g_worker.reset();
        g_http.reset();
        wc::Host::Detach();
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    g_definition.Signature = 0x57434E58;  // "WCNX": the number Nexus knows the addon by. it never changes
    g_definition.APIVersion = NEXUS_API_VERSION;
    g_definition.Name = "Wondrous Compass";
    g_definition.Version.Major = WC_VERSION_MAJOR;
    g_definition.Version.Minor = WC_VERSION_MINOR;
    g_definition.Version.Build = WC_VERSION_PATCH;
    g_definition.Version.Revision = 0;
    g_definition.Author = "Yesanith";
    g_definition.Description = "Public and guild events, sign ups and guild recruitment of Wondrous Compass, in the game.";
    g_definition.Load = Load;
    g_definition.Unload = Unload;
    g_definition.Flags = AF_None;
    g_definition.Provider = UP_GitHub;
    g_definition.UpdateLink = "https://github.com/Yesanith/wc-nexus";
    return &g_definition;
}
