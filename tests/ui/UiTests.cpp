// draws the window of the addon off the game, with a pretend server, through every page: ImGui checks that each Begin has its End and each
// push its pop, and the frames are drawn into pictures (tests/ui/out/*.ppm) by a small rasterizer, to look at the layout without the game
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "addon/Ui.h"
#include "core/Text.h"
#include "imgui.h"

namespace wc::Host {
    extern std::vector<std::string> g_alerts;
    extern std::filesystem::path g_folder;
}

namespace wc::ui {
    struct UiTestAccess {
        static void Show(App& a, Page p) { a.Show(p); }
        static bool Busy(App& a) {
            return a.m_loading || a.m_finding || a.m_myGuildsLoading || a.m_rosterLoading || a.m_pageLoading || a.m_linking || a.m_creating ||
                   !a.m_busy.empty();
        }
        static void OpenDetails(App& a, bool guild) { a.OpenDetails(guild ? a.m_guild.at(0) : a.m_open.at(0)); }
        static void OpenGuild(App& a) { a.OpenGuild(a.m_found.rows.at(0)); }
        static void OpenEditor(App& a) { a.OpenEditor(a.m_myGuilds.at(0)); }
        static void Edit(App& a) {
            a.EditEvent(a.m_guild.at(0));
            a.Show(Page::Create);
        }
        static void DraftForm(App& a) {
            a.m_draft = EventDraft();
            a.m_draft.title = "Fractal CMs";
            a.m_draft.whenText = "+1d 20:00";
            a.m_draft.UsePreset("Party of 5");
        }
        static void Create(App& a) { a.CreateNow(); }
        static std::string Status(App& a) { return a.m_createStatus.text; }
        static void Join(App& a) { a.Join(a.m_open.at(1), "dps", "going"); }
        static bool Linked(App& a) { return a.m_api->Linked(); }
        static size_t Lists(App& a) { return a.m_open.size() + a.m_guild.size() + a.m_mine.size(); }
        static void Link(App& a, const std::string& code) {
            a.m_code = code;
            a.LinkNow();
        }
        static std::string LinkStatus(App& a) { return a.m_linkStatus.text; }
    };
}

using namespace wc;
using namespace wc::ui;

namespace {
    int g_failures = 0;
    void Check(bool ok, const std::string& what) {
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
        if (!ok) ++g_failures;
    }

    std::string Iso(long long fromNow) {
        std::time_t t = std::time(nullptr) + fromNow;
        return Clock::ToIsoUtc(t);
    }

    /// a pretend server, which answers what the window asks with a few events, guilds and pages
    struct FakeServer : IHttp {
        std::vector<std::string> calls;
        HttpResponse Post(const std::string& url, const std::map<std::string, std::string>&, const std::string& body) override {
            std::string fn = url.substr(url.rfind('/') + 1);
            calls.push_back(fn);
            Json args = Json::parse(body);
            auto ok = [](const std::string& s) { return HttpResponse{200, s, ""}; };
            if (fn == "module_link_exchange") return ok(R"({"token":"tcm_)" + std::string(64, 'a') + R"(","name":"Ada"})");
            if (fn == "module_me")
                return ok(R"({"name":"Ada","accounts":[{"id":"a1","name":"Ada.1234","region":"eu"}],
                    "guilds":[{"guild_id":"g1","name":"Order of Whispers Fan Club","tag":"OWF","role":"leader","account_id":"a1","account_name":"Ada.1234"},
                              {"guild_id":"g2","name":"Lionguard","tag":"LG","role":"member","account_id":"a1","account_name":"Ada.1234"}]})");
            if (fn == "module_open_events" || fn == "module_my_open_events") {
                Json rows = Json::array();
                rows.push_back(Json{{"id", "e1"}, {"title", "Fractal CMs, all three, experienced"}, {"activity", "fractal"}, {"language", "en"},
                                    {"region", "eu"}, {"start_at", Iso(3 * 3600)}, {"duration_minutes", 90}, {"max_participants", 5},
                                    {"organizer", "Ada.1234"}, {"mine", true}, {"going", 3}, {"maybe", 1}, {"my_status", "going"}, {"my_role", "healer"},
                                    {"roles", Json::array({Json{{"role", "healer"}, {"slots", 1}, {"taken", 1}}, Json{{"role", "boon_dps"}, {"slots", 1}, {"taken", 1}},
                                                           Json{{"role", "dps"}, {"slots", 3}, {"taken", 1}}})},
                                    {"tags", Json::array({"meta"})}, {"map_id", 1234},
                                    {"description", "Bring your own food and utility. We meet at the Mistlock Observatory."}});
                if (fn == "module_open_events") {
                    rows.push_back(Json{{"id", "e2"}, {"title", u8"Шатерер — утренний забег"}, {"activity", "worldboss"}, {"language", "ru"},
                                        {"region", "all"}, {"start_at", Iso(26 * 3600)}, {"duration_minutes", 30}, {"organizer", "Iva.5678"},
                                        {"going", 12}, {"roles", Json::array({Json{{"role", "dps"}, {"slots", 20}, {"taken", 12}}})}});
                    rows.push_back(Json{{"id", "e3"}, {"title", "Old raid"}, {"activity", "raid"}, {"start_at", Iso(-5 * 3600)}, {"duration_minutes", 60},
                                        {"going", 10}, {"max_participants", 10}});
                }
                return ok(rows.dump());
            }
            if (fn == "module_guild_events") {
                Json rows = Json::array();
                rows.push_back(Json{{"id", "g-e1"}, {"guild_id", "g1"}, {"guild_name", "Order of Whispers Fan Club"}, {"guild_tag", "OWF"},
                                    {"title", "Raid night"}, {"activity", "raid"}, {"start_at", Iso(50 * 3600)}, {"duration_minutes", 180},
                                    {"organizer", "Ada.1234"}, {"mine", true}, {"going", 7}, {"waitlist", 2}, {"my_status", "going"}, {"my_role", "healer"},
                                    {"roles", Json{{"tank", 1}, {"healer", 2}, {"dps", 7}}}, {"version", 3}, {"series_id", "s1"}, {"can_manage", true}});
                return ok(rows.dump());
            }
            if (fn == "module_open_event_roster" || fn == "module_event_roster")
                return ok(R"([{"name":"Ada.1234","status":"going","role":"healer","profession":"Druid","flex_roles":["dps"]},
                             {"name":"Iva.5678","status":"maybe","role":"dps","profession":"Thief"}])");
            if (fn == "recruiting_guilds")
                return ok(R"({"total":2,"rows":[{"guild_id":"r1","name":"Dragon Hunters","tag":"DH","headline":"Casual raids and fractals every week",
                    "looking_for":["raids","fractals","newcomers"],"languages":["en"],"region":"eu","recruiting":true,"play_times":"Tue/Thu 20:00 CET"},
                    {"guild_id":"r2","name":"Quiet Ones","tag":"QO","headline":"Social guild","looking_for":["social"],"languages":["de"],"region":"na","recruiting":false}]})");
            if (fn == "module_recruitment_page")
                return ok(R"({"guild_id":"g1","name":"Order of Whispers Fan Club","tag":"OWF","headline":"Casual raids every week",
                    "description":"We are a friendly guild that raids twice a week and runs fractals on weekends.","requirements":"Level 80",
                    "play_times":"Tue/Thu 20:00 CET","looking_for":["raids","fractals"],"languages":["en"],"region":"eu","recruiting":true,
                    "discord_invite":"https://discord.gg/abc","contact":"Ada.1234","can_edit":true,"published":true})");
            if (fn == "module_my_recruitment_guilds")
                return ok(R"([{"guild_id":"g1","name":"Order of Whispers Fan Club","tag":"OWF","is_leader":true,"has_page":true,"published":true}])");
            if (fn == "module_guild_recruiters") return ok(R"([{"user_id":"u2","account_name":"Iva.5678","role":"member"}])");
            if (fn == "module_guild_roster")
                return ok(R"([{"user_id":"u1","account_name":"Ada.1234","role":"leader"},{"user_id":"u2","account_name":"Iva.5678","role":"member"},
                             {"user_id":"u3","account_name":"Bo.9012","role":"member"}])");
            if (fn == "module_create_open_event" || fn == "module_create_event") return ok("\"new-id\"");
            return ok("null");
        }
    };

    // ---- a small rasterizer of ImGui's triangles, enough to see a frame

    struct Canvas {
        int w, h;
        std::vector<float> rgb;
        const unsigned char* atlas = nullptr;
        int aw = 0, ah = 0;

        Canvas(int w, int h) : w(w), h(h), rgb(static_cast<size_t>(w * h * 3), 0.0f) {}

        void Clear(float r, float g, float b) {
            for (int i = 0; i < w * h; ++i) {
                rgb[i * 3] = r;
                rgb[i * 3 + 1] = g;
                rgb[i * 3 + 2] = b;
            }
        }

        static float Edge(const ImVec2& a, const ImVec2& b, float x, float y) { return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x); }

        void Triangle(const ImDrawVert& v0, const ImDrawVert& v1, const ImDrawVert& v2, const ImVec4& clip) {
            float area = Edge(v0.pos, v1.pos, v2.pos.x, v2.pos.y);
            if (std::fabs(area) < 1e-6f) return;
            int x0 = std::max(static_cast<int>(std::floor(std::min({v0.pos.x, v1.pos.x, v2.pos.x}))), std::max(0, static_cast<int>(clip.x)));
            int x1 = std::min(static_cast<int>(std::ceil(std::max({v0.pos.x, v1.pos.x, v2.pos.x}))), std::min(w - 1, static_cast<int>(clip.z)));
            int y0 = std::max(static_cast<int>(std::floor(std::min({v0.pos.y, v1.pos.y, v2.pos.y}))), std::max(0, static_cast<int>(clip.y)));
            int y1 = std::min(static_cast<int>(std::ceil(std::max({v0.pos.y, v1.pos.y, v2.pos.y}))), std::min(h - 1, static_cast<int>(clip.w)));
            auto unpack = [](ImU32 c, int shift) { return ((c >> shift) & 0xFF) / 255.0f; };
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    float px = x + 0.5f, py = y + 0.5f;
                    float b0 = Edge(v1.pos, v2.pos, px, py) / area, b1 = Edge(v2.pos, v0.pos, px, py) / area, b2 = Edge(v0.pos, v1.pos, px, py) / area;
                    if (b0 < 0 || b1 < 0 || b2 < 0) continue;
                    float u = b0 * v0.uv.x + b1 * v1.uv.x + b2 * v2.uv.x, v = b0 * v0.uv.y + b1 * v1.uv.y + b2 * v2.uv.y;
                    int tx = std::min(aw - 1, std::max(0, static_cast<int>(u * aw))), ty = std::min(ah - 1, std::max(0, static_cast<int>(v * ah)));
                    float tex = atlas[ty * aw + tx] / 255.0f;
                    float a = (b0 * unpack(v0.col, 24) + b1 * unpack(v1.col, 24) + b2 * unpack(v2.col, 24)) * tex;
                    float* p = &rgb[(y * w + x) * 3];
                    for (int k = 0; k < 3; ++k) {
                        float c = b0 * unpack(v0.col, k * 8) + b1 * unpack(v1.col, k * 8) + b2 * unpack(v2.col, k * 8);
                        p[k] = p[k] * (1 - a) + c * a;
                    }
                }
            }
        }

        void Draw(ImDrawData* data) {
            for (int n = 0; n < data->CmdListsCount; ++n) {
                const ImDrawList* list = data->CmdLists[n];
                for (const ImDrawCmd& cmd : list->CmdBuffer) {
                    if (cmd.UserCallback) continue;
                    for (unsigned i = 0; i < cmd.ElemCount; i += 3) {
                        const ImDrawIdx* idx = list->IdxBuffer.Data + cmd.IdxOffset + i;
                        const ImDrawVert* vtx = list->VtxBuffer.Data + cmd.VtxOffset;
                        Triangle(vtx[idx[0]], vtx[idx[1]], vtx[idx[2]], cmd.ClipRect);
                    }
                }
            }
        }

        void Save(const std::filesystem::path& path) const {
            std::ofstream out(path, std::ios::binary);
            out << "P6\n" << w << " " << h << "\n255\n";
            for (float c : rgb) out.put(static_cast<char>(std::min(255, std::max(0, static_cast<int>(c * 255.0f + 0.5f)))));
        }
    };

    struct Harness {
        std::shared_ptr<FakeServer> server = std::make_shared<FakeServer>();
        std::shared_ptr<CodexApi> api;
        Worker worker;
        std::unique_ptr<App> app;
        Canvas canvas{1100, 760};
        std::filesystem::path out;

        explicit Harness(bool linked) {
            api = std::make_shared<CodexApi>(server, "key", linked ? "tcm_" + std::string(64, 'a') : "", "https://example.test");
            worker.Start();
            app = std::make_unique<App>(api, worker, TokenStore(Host::g_folder), Host::g_folder);
        }
        ~Harness() { worker.Stop(); }

        void Frame(ImVec2 mouse = ImVec2(-1, -1), bool down = false) {
            ImGuiIO& io = ImGui::GetIO();
            io.DeltaTime = 1.0f / 60.0f;
            io.MousePos = mouse;
            io.MouseDown[0] = down;
            ImGui::NewFrame();
            app->Frame();
            ImGui::Render();
        }

        /// frames until what the worker does has come back
        void Settle() {
            for (int i = 0; i < 400; ++i) {
                Frame();
                if (!UiTestAccess::Busy(*app) && i > 3) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            Frame();
            Frame();
        }

        void Shot(const std::string& name) {
            canvas.Clear(0.08f, 0.08f, 0.1f);
            canvas.Draw(ImGui::GetDrawData());
            canvas.Save(out / (name + ".ppm"));
        }
    };
}

int main() {
#ifdef _WIN32
    _putenv_s("TZ", "UTC");
#else
    setenv("TZ", "UTC", 1);
    tzset();
#endif
    Host::g_folder = std::filesystem::temp_directory_path() / ("wc-ui-" + std::to_string(std::time(nullptr)));
    std::filesystem::create_directories(Host::g_folder);
    std::filesystem::path out = std::filesystem::path(WC_SOURCE_DIR) / "tests" / "ui" / "out";
    std::filesystem::create_directories(out);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1100, 760);
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0100, 0x024F, 0x0400, 0x04FF, 0x2010, 0x2027, 0};
    const char* font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if (std::filesystem::exists(font)) io.Fonts->AddFontFromFileTTF(font, 16.0f, nullptr, ranges);
    else io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int aw = 0, ah = 0;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &aw, &ah);

    try {
        {
            // not linked: the window opens on the account page, and a code links it
            Harness h(false);
            h.out = out;
            h.canvas.atlas = pixels;
            h.canvas.aw = aw;
            h.canvas.ah = ah;
            Loc::Use("en");
            h.app->Toggle();
            h.Settle();
            h.Shot("01-account-unlinked");
            UiTestAccess::Link(*h.app, "ABCD-EFGH");
            h.Settle();
            Check(UiTestAccess::Linked(*h.app), "a code links the addon");
            Check(UiTestAccess::LinkStatus(*h.app) == "Linked as Ada.", "and says who it is linked as");
            Check(UiTestAccess::Lists(*h.app) > 0, "and the lists are read");
        }
        {
            Harness h(true);
            h.out = out;
            h.canvas.atlas = pixels;
            h.canvas.aw = aw;
            h.canvas.ah = ah;
            Loc::Use("en");
            h.app->Toggle();
            h.Settle();
            h.Shot("02-public");
            UiTestAccess::Show(*h.app, Page::Guild);
            h.Settle();
            h.Shot("03-guild");
            UiTestAccess::Show(*h.app, Page::Mine);
            h.Settle();
            h.Shot("04-mine");
            UiTestAccess::OpenDetails(*h.app, false);
            h.Settle();
            h.Shot("05-details-public");
            UiTestAccess::OpenDetails(*h.app, true);
            h.Settle();
            h.Shot("06-details-guild");
            UiTestAccess::Show(*h.app, Page::Create);
            UiTestAccess::DraftForm(*h.app);
            h.Settle();
            h.Shot("07-create");
            UiTestAccess::Create(*h.app);
            h.Settle();
            Check(UiTestAccess::Status(*h.app) == "Event made.", "an event is made");
            UiTestAccess::Edit(*h.app);
            h.Settle();
            h.Shot("08-edit-guild-event");
            UiTestAccess::Show(*h.app, Page::Find);
            h.Settle();
            h.Shot("09-find");
            UiTestAccess::OpenGuild(*h.app);
            h.Settle();
            h.Shot("10-guild-page");
            UiTestAccess::Show(*h.app, Page::Recruit);
            h.Settle();
            h.Shot("11-recruit");
            UiTestAccess::OpenEditor(*h.app);
            h.Settle();
            h.Settle();
            h.Shot("12-recruit-editor");
            UiTestAccess::Show(*h.app, Page::Account);
            h.Settle();
            h.Shot("13-account");
            UiTestAccess::Join(*h.app);
            h.Settle();
            bool joined = std::find(h.server->calls.begin(), h.server->calls.end(), "module_join_open_event") != h.server->calls.end();
            Check(joined, "signing up calls the server");

            // the same window in Turkish and Russian, for the length of the words
            Loc::Use("tr");
            UiTestAccess::Show(*h.app, Page::Public);
            h.Settle();
            h.Shot("14-public-tr");
            Loc::Use("ru");
            UiTestAccess::Show(*h.app, Page::Create);
            h.Settle();
            h.Shot("15-create-ru");
            Loc::Use("en");

            // a click on a tab: the second tab is where the first tab ends
            h.Frame(ImVec2(140, 46), true);
            h.Frame(ImVec2(140, 46), false);
            h.Settle();
        }
        Check(true, "every page was drawn without an error of ImGui");
    } catch (const std::exception& e) {
        Check(false, std::string("ImGui stopped: ") + e.what());
    }
    ImGui::DestroyContext();
    std::error_code ec;
    std::filesystem::remove_all(Host::g_folder, ec);
    std::printf("%d failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
