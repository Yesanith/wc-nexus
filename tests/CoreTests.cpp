#include <filesystem>
#include <fstream>
#include <functional>

#include "core/Api.h"
#include "core/Drafts.h"
#include "core/Rules.h"
#include "core/Support.h"
#include "core/Text.h"
#include "core/Time.h"
#include "doctest/doctest.h"

using namespace wc;

namespace {
    // the tests run in UTC (tests/Main.cpp), so a local time is the same as a UTC one
    Moment At(int y, int mo, int d, int h, int mi, int s = 0) { return Clock::FromLocal(y, mo, d, h, mi, s); }

    const Moment Now = At(2026, 10, 9, 18, 30);
    const std::string Token = "tcm_" + std::string(64, 'c');

    /// a pretend server: it answers each call with what [answer] says and remembers what it was asked
    struct FakeServer : IHttp {
        struct Call {
            std::string url;
            Json body;
            std::string key;
        };
        std::vector<Call> calls;
        std::function<HttpResponse(const std::string&, const Json&)> answer = [](const std::string&, const Json&) { return HttpResponse{200, "null", ""}; };

        HttpResponse Post(const std::string& url, const std::map<std::string, std::string>& headers, const std::string& body) override {
            Json j = Json::parse(body);
            auto key = headers.find("apikey");
            calls.push_back({url, j, key == headers.end() ? "" : key->second});
            return answer(url.substr(url.rfind('/') + 1), j);
        }
    };

    std::pair<std::shared_ptr<CodexApi>, std::shared_ptr<FakeServer>> Make(const std::string& token = "") {
        auto server = std::make_shared<FakeServer>();
        return {std::make_shared<CodexApi>(server, "sb_publishable_key", token, "https://example.test/"), server};
    }

    HttpResponse Ok(const std::string& body) { return {200, body, ""}; }

    struct TempDir {
        std::filesystem::path path;
        TempDir() {
            path = std::filesystem::temp_directory_path() / ("wc-test-" + std::to_string(std::rand()) + "-" + std::to_string(std::time(nullptr)));
            std::filesystem::create_directories(path);
        }
        ~TempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };

    EventDraft Good() {
        EventDraft d;
        d.title = "Fractals";
        d.whenText = "20:00";
        d.activity = "fractal";
        return d;
    }
}

TEST_SUITE("guilds") {
    TEST_CASE("only leaders and officers may create events") {
        for (auto [role, expected] : std::vector<std::pair<std::string, bool>>{{"leader", true}, {"officer", true}, {"member", false}, {"", false}}) {
            GuildInfo g;
            g.role = role;
            CHECK(g.CanCreate() == expected);
        }
    }

    TEST_CASE("what the server says of making events decides") {
        GuildInfo member;
        member.role = "member";
        member.canCreate = true;
        CHECK(member.CanCreate());
        GuildInfo officer;
        officer.role = "officer";
        officer.canCreate = false;
        CHECK_FALSE(officer.CanCreate());
    }
}

TEST_SUITE("time input") {
    TEST_CASE("reads what a player types") {
        struct Case {
            const char* text;
            int y, mo, d, h, mi;
        };
        for (const auto& c : std::vector<Case>{{"20:00", 2026, 10, 9, 20, 0},       {"8", 2026, 10, 10, 8, 0},
                                               {"18:30", 2026, 10, 10, 18, 30},     {"+1d 20:00", 2026, 10, 10, 20, 0},
                                               {"+2D 7:15", 2026, 10, 11, 7, 15},   {"tomorrow 21:30", 2026, 10, 10, 21, 30},
                                               {"today 23:00", 2026, 10, 9, 23, 0}, {"2026-12-24 19:00", 2026, 12, 24, 19, 0},
                                               {"10-12 20:00", 2026, 10, 12, 20, 0}, {"  20:00  ", 2026, 10, 9, 20, 0},
                                               {u8"yarın 21:30", 2026, 10, 10, 21, 30}, {u8"завтра 21:30", 2026, 10, 10, 21, 30},
                                               {u8"明日 21:30", 2026, 10, 10, 21, 30}, {"morgen 21:30", 2026, 10, 10, 21, 30},
                                               {"aujourd'hui 21:30", 2026, 10, 9, 21, 30}, {u8"aujourd\u2019hui 21:30", 2026, 10, 9, 21, 30}}) {
            CAPTURE(c.text);
            auto at = TimeInput::Parse(c.text, Now);
            REQUIRE(at.has_value());
            CHECK(*at == At(c.y, c.mo, c.d, c.h, c.mi));
        }
    }

    TEST_CASE("a date that has gone by is next year") { CHECK(TimeInput::Parse("3-1 20:00", Now) == At(2027, 3, 1, 20, 0)); }

    TEST_CASE("refuses what is not a time") {
        for (const char* text : {"", "   ", "soon", "25:00", "20:61", "+1d 99:00", "2026-13-40 20:00", "10-12 nonsense", "+d 20:00"}) {
            CAPTURE(text);
            CHECK_FALSE(TimeInput::Parse(text, Now).has_value());
        }
    }

    TEST_CASE("describes today, tomorrow and later") {
        Loc::Use("en");
        CHECK(TimeInput::Describe(At(2026, 10, 9, 20, 0), Now) == "Today 20:00");
        CHECK(TimeInput::Describe(At(2026, 10, 10, 9, 5), Now) == "Tomorrow 09:05");
        CHECK(TimeInput::Describe(At(2026, 10, 12, 20, 0), Now) == "Mon 12 Oct 20:00");
    }

    TEST_CASE("reads the times the server writes") {
        CHECK(Clock::ParseIso("2026-10-09T20:00:00Z") == At(2026, 10, 9, 20, 0));
        CHECK(Clock::ParseIso("2026-10-09T20:00:00+00:00") == At(2026, 10, 9, 20, 0));
        CHECK(Clock::ParseIso("2026-10-09T22:00:00+02:00") == At(2026, 10, 9, 20, 0));
        CHECK(Clock::ParseIso("2026-10-09T20:00:00.123456+00:00") == At(2026, 10, 9, 20, 0));
        CHECK(Clock::ParseIso("2026-10-09 19:30:00-00:30") == At(2026, 10, 9, 20, 0));
        CHECK_FALSE(Clock::ParseIso("yesterday").has_value());
        CHECK(Clock::ToIsoUtc(At(2026, 10, 9, 20, 0)) == "2026-10-09T20:00:00Z");
    }
}

TEST_SUITE("token") {
    TEST_CASE("keeps and gives back a token, and forgets it") {
        TempDir dir;
        TokenStore store(dir.path);
        CHECK(store.Load().empty());
        CHECK(store.Save(Token));
        CHECK(store.Load() == Token);
        store.Clear();
        CHECK(store.Load().empty());
    }

    TEST_CASE("a rubbish file is no token") {
        TempDir dir;
        std::ofstream(dir.path / "link.token") << "not a token";
        CHECK(TokenStore(dir.path).Load().empty());
    }

    TEST_CASE("refuses to keep what is not one, and knows a token") {
        TempDir dir;
        CHECK_FALSE(TokenStore(dir.path).Save("tcm_short"));
        CHECK(TokenStore::IsToken(Token));
        CHECK_FALSE(TokenStore::IsToken("tcm_" + std::string(64, 'G')));
        CHECK_FALSE(TokenStore::IsToken("xyz_" + std::string(64, 'a')));
        CHECK_FALSE(TokenStore::IsToken(""));
    }
}

TEST_SUITE("api") {
    TEST_CASE("linking keeps the token and sends the key") {
        auto [api, server] = Make();
        server->answer = [](const std::string&, const Json&) { return Ok("{\"token\":\"" + Token + "\",\"name\":\"Ada\",\"avatar_url\":null}"); };
        LinkResult result = api->Link("ABCD-EFGH", "Nexus");
        CHECK(result.Ok());
        CHECK(result.name == "Ada");
        CHECK(api->Token() == Token);
        CHECK(api->Linked());
        REQUIRE(server->calls.size() == 1);
        CHECK(server->calls[0].url == "https://example.test/rest/v1/rpc/module_link_exchange");
        CHECK(server->calls[0].key == "sb_publishable_key");
        CHECK(server->calls[0].body["p_code"] == "ABCD-EFGH");
        CHECK(server->calls[0].body["p_label"] == "Nexus");
        CHECK_FALSE(server->calls[0].body.contains("p_token"));
    }

    TEST_CASE("a wrong code leaves it unlinked") {
        Loc::Use("en");
        auto [api, server] = Make();
        server->answer = [](const std::string&, const Json&) { return Ok("{\"error\":\"bad_code\"}"); };
        LinkResult result = api->Link("NOPE-NOPE", "x");
        CHECK_FALSE(result.Ok());
        CHECK(result.error == "bad_code");
        CHECK_FALSE(api->Linked());
        CHECK(ErrorText::ForLink(result.error).find("wrong") != std::string::npos);
    }

    TEST_CASE("every call carries the token") {
        auto [api, server] = Make(Token);
        server->answer = [](const std::string&, const Json&) { return Ok("[]"); };
        EventFilter f;
        f.activity = "raid";
        f.language = "en";
        api->OpenEvents(f, 10, 0);
        api->GuildEvents();
        api->MyOpenEvents();
        api->OpenRoster("e1");
        api->GuildRoster("e2");
        for (const auto& c : server->calls) CHECK(c.body["p_token"] == Token);
        CHECK(server->calls[0].body["p_activity"] == "raid");
        CHECK(server->calls[0].body["p_region"].is_null());
        CHECK(server->calls[3].body["p_event"] == "e1");
    }

    TEST_CASE("a call without a link is refused before it goes out") {
        auto [api, server] = Make();
        try {
            api->Me();
            FAIL("no error");
        } catch (const ApiError& e) {
            CHECK(e.IsInvalidToken());
        }
        CHECK(server->calls.empty());
    }

    TEST_CASE("the hint of the database is the code") {
        Loc::Use("en");
        auto [api, server] = Make(Token);
        server->answer = [](const std::string&, const Json&) {
            return HttpResponse{400, "{\"code\":\"P0001\",\"message\":\"role is full\",\"hint\":\"role_full\"}", ""};
        };
        try {
            api->JoinOpen("e", "going", "", "dps", "Thief");
            FAIL("no error");
        } catch (const ApiError& e) {
            CHECK(e.code == "role_full");
            CHECK(ErrorText::For(e) == "That role is full.");
        }
        CHECK(server->calls[0].body["p_account"].is_null());
    }

    TEST_CASE("an answer that is not json is named, and an unreached server is told") {
        Loc::Use("en");
        auto [api, server] = Make(Token);
        server->answer = [](const std::string&, const Json&) { return HttpResponse{500, "<html>", ""}; };
        try {
            api->Me();
            FAIL("no error");
        } catch (const ApiError& e) {
            CHECK(e.code == "http_500");
            CHECK(ErrorText::For(e).find("server") != std::string::npos);
        }
        server->answer = [](const std::string&, const Json&) { return Ok("<html>"); };
        try {
            api->Me();
            FAIL("no error");
        } catch (const ApiError& e) {
            CHECK(e.code == "bad_answer");
        }
        server->answer = [](const std::string&, const Json&) { return HttpResponse{0, "", "timed out"}; };
        try {
            api->Me();
            FAIL("no error");
        } catch (const ApiError& e) {
            CHECK(e.IsNetwork());
            CHECK(ErrorText::For(e) == "Could not reach the server. Check the connection.");
        }
    }

    TEST_CASE("makes a public event the way the backend takes it") {
        auto [api, server] = Make(Token);
        server->answer = [](const std::string&, const Json&) { return Ok("\"11111111-1111-1111-1111-111111111111\""); };
        NewOpenEvent e;
        e.title = "Fractals";
        e.startAt = At(2026, 10, 9, 20, 0);
        e.durationMinutes = 90;
        e.activity = "fractal";
        e.maxParticipants = 5;
        e.roles = EventRules::Preset("Party of 5");
        e.myRole = "dps";
        e.myProfession = "Thief";
        CHECK(api->CreateOpen(e) == "11111111-1111-1111-1111-111111111111");
        const Json& body = server->calls.at(0).body;
        CHECK(server->calls[0].url.substr(server->calls[0].url.rfind('/') + 1) == "module_create_open_event");
        CHECK(body["p_start_at"] == "2026-10-09T20:00:00Z");
        CHECK(body["p_duration_minutes"] == 90);
        CHECK(body["p_max_participants"] == 5);
        CHECK(body["p_my_role"] == "dps");
        CHECK(body["p_roles"].size() == 3);
        CHECK(body["p_roles"][0]["role"] == "healer");
        CHECK(body["p_roles"][0]["slots"] == 1);
        CHECK(body["p_map_id"].is_null());
    }

    TEST_CASE("makes a guild event, and a weekly one") {
        auto [api, server] = Make(Token);
        server->answer = [](const std::string&, const Json&) { return Ok("\"22222222-2222-2222-2222-222222222222\""); };
        NewGuildEvent e;
        e.guildId = "g1";
        e.title = "Raid night";
        e.startAt = Now + 3 * 3600;
        e.accountId = "a1";
        api->CreateGuild(e);
        const Json& body = server->calls.at(0).body;
        CHECK(body["p_guild"] == "g1");
        CHECK(body["p_account"] == "a1");
        CHECK(body["p_roles"].empty());
        CHECK(body["p_max_participants"].is_null());

        WeeklySlot slot = WeeklySlot::Of(At(2026, 10, 9, 20, 0), "W. Europe Standard Time");
        api->CreateGuildSeries(e, slot);
        const Json& weekly = server->calls.at(1).body;
        CHECK(weekly["p_weekday"] == 5);
        CHECK(weekly["p_time_of_day"] == "20:00");
        CHECK(weekly["p_timezone"] == "Europe/Berlin");
    }

    TEST_CASE("roles without places are left out") {
        Json roles = CodexApi::RolesJson({{"tank", 0}, {"dps", 4}});
        REQUIRE(roles.size() == 1);
        CHECK(roles[0]["role"] == "dps");
    }

    TEST_CASE("an event that is changed keeps its length, also one the form does not offer") {
        EventInfo e;
        e.id = "e1";
        e.title = "Long meta";
        e.startAt = At(2026, 10, 10, 20, 0);
        e.durationMinutes = 150;
        CHECK(EventDraft::FromEvent(e).durationMinutes == 150);
        e.durationMinutes = 720;
        CHECK(EventDraft::FromEvent(e).durationMinutes == 720);
    }

    TEST_CASE("a Discord invite is taken as the database takes it, a slash at the end left off") {
        CHECK(IsDiscordInvite("https://discord.gg/abc"));
        CHECK(IsDiscordInvite("https://discord.gg/abc/"));
        CHECK_FALSE(IsDiscordInvite("https://discord.gg/a"));
        CHECK_FALSE(IsDiscordInvite("https://discord.gg/" + std::string(41, 'a')));
        RecruitDraft d;
        d.discordInvite = "https://discord.com/invite/abc/";
        CHECK(d.ToJson()["discord_invite"] == "https://discord.com/invite/abc");
    }

    TEST_CASE("a changed guild event is sent with the version it was read at") {
        auto [api, server] = Make(Token);
        server->answer = [](const std::string&, const Json&) { return Ok("4"); };
        EventInfo e;
        e.id = "e1";
        e.isGuild = true;
        e.guildId = "g1";
        e.title = "Raid";
        e.startAt = At(2026, 10, 10, 20, 0);
        e.durationMinutes = 120;
        e.version = 3;
        EventDraft d = EventDraft::FromEvent(e);
        CHECK(api->UpdateGuildEvent(d, Now) == 4);
        CHECK(server->calls[0].body["p_version"] == 3);
        CHECK(server->calls[0].body["p_event"] == "e1");
    }

    TEST_CASE("recruiting guilds are asked without the token") {
        auto [api, server] = Make();
        server->answer = [](const std::string&, const Json&) {
            return Ok("{\"total\":2,\"rows\":[{\"guild_id\":\"g1\",\"name\":\"Order\",\"tag\":\"OR\",\"headline\":\"Hi\",\"looking_for\":[\"raids\"],\"languages\":[\"en\"],"
                      "\"region\":\"eu\",\"recruiting\":false},{\"guild_id\":\"\",\"name\":\"no id\"}]}");
        };
        RecruitFilter f;
        f.search = "  ord ";
        RecruitList list = api->RecruitingGuilds(f);
        CHECK(list.total == 2);
        REQUIRE(list.rows.size() == 1);
        CHECK(list.rows[0].Title() == "Order [OR]");
        CHECK_FALSE(list.rows[0].recruiting);
        CHECK(server->calls[0].body["p_search"] == "ord");
        CHECK_FALSE(server->calls[0].body.contains("p_token"));
    }
}

TEST_SUITE("parsing") {
    TEST_CASE("reads who the player is") {
        MeInfo me = MeInfo::Parse(Json::parse(R"({"name":"Ada","accounts":[{"id":"a1","name":"Ada.1234","region":"eu"}],
            "guilds":[{"guild_id":"g1","name":"Order","tag":"OR","role":"officer","account_id":"a1","account_name":"Ada.1234"}]})"));
        CHECK(me.name == "Ada");
        REQUIRE(me.accounts.size() == 1);
        CHECK(me.accounts[0].region == "eu");
        REQUIRE(me.guilds.size() == 1);
        CHECK(me.guilds[0].Label() == "Order [OR]");
        CHECK(me.guilds[0].CanCreate());

        MeInfo let = MeInfo::Parse(Json::parse(R"({"guilds":[{"guild_id":"g1","role":"member","can_create":true},{"guild_id":"g2","role":"member","can_create":false},{"guild_id":"g3","role":"member"}]})"));
        REQUIRE(let.guilds.size() == 3);
        CHECK(let.guilds[0].CanCreate());
        CHECK_FALSE(let.guilds[1].CanCreate());
        CHECK_FALSE(let.guilds[2].CanCreate());
    }

    TEST_CASE("reads a public event") {
        EventInfo e = EventInfo::ParseOpen(Json::parse(R"({"id":"e1","title":"Fractals","activity":"fractal","language":"en","region":"eu",
            "start_at":"2026-10-09T20:00:00+00:00","duration_minutes":90,"max_participants":5,"organizer":"Ada","mine":true,"going":2,"maybe":1,
            "my_status":"going","my_role":"dps","roles":[{"role":"dps","slots":3,"taken":3}],"tags":["meta"],"map_id":1234})"));
        CHECK(e.title == "Fractals");
        CHECK(e.startAt == At(2026, 10, 9, 20, 0));
        CHECK(e.Places() == "2/5");
        CHECK(e.Joined());
        CHECK(e.roles.at(0).Full());
        CHECK(e.mapId == 1234);
        CHECK(e.tags == std::vector<std::string>{"meta"});
    }

    TEST_CASE("reads a guild event, with the roles in the order the server wrote them") {
        EventInfo e = EventInfo::ParseGuild(Json::parse(R"({"id":"e2","guild_id":"g1","guild_name":"Order","guild_tag":"OR","title":"Raid",
            "start_at":"2026-10-09T20:00:00Z","duration_minutes":120,"roles":{"tank":1,"healer":2,"dps":7},"version":3,"series_id":"s1",
            "can_manage":true,"my_flex":["healer"],"waitlist":2})"));
        CHECK(e.isGuild);
        CHECK(e.guildLabel == "Order [OR]");
        REQUIRE(e.roles.size() == 3);
        CHECK(e.roles[0].role == "tank");
        CHECK(e.roles[2].slots == 7);
        CHECK(e.version == 3);
        CHECK(e.seriesId == "s1");
        CHECK(e.CanChange());
        CHECK(e.myFlex == std::vector<std::string>{"healer"});
    }

    TEST_CASE("an event with nothing in it still reads") {
        EventInfo e = EventInfo::ParseOpen(Json::object());
        CHECK(e.activity == "other");
        CHECK(e.status == "scheduled");
        CHECK(e.Places() == "0");
        CHECK_FALSE(e.Joined());
    }

    TEST_CASE("reads a roster") {
        auto rows = RosterEntry::ParseAll(Json::parse(R"([{"name":"Ada","status":"going","role":"dps","profession":"Thief","flex_roles":["healer"]}])"));
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].flex == std::vector<std::string>{"healer"});
    }

    TEST_CASE("an event is over when its length is up, and one with no length where it begins") {
        EventInfo e;
        e.startAt = Now;
        e.durationMinutes = 60;
        CHECK_FALSE(e.Ended(Now + 59 * 60));
        CHECK(e.Ended(Now + 60 * 60));
        e.durationMinutes = 0;
        CHECK(e.Ended(Now));
    }

    TEST_CASE("reads a page and where the pages of my guilds are") {
        auto page = RecruitPage::Parse(Json::parse(R"({"guild_id":"g1","name":"Order","headline":"Hi","can_edit":true,"published":true,"discord_invite":null})"));
        REQUIRE(page.has_value());
        CHECK(page->canEdit);
        CHECK(page->recruiting);
        CHECK(page->discordInvite.empty());
        CHECK_FALSE(RecruitPage::Parse(Json(nullptr)).has_value());
        Loc::Use("en");
        auto mine = MyRecruitGuild::ParseAll(Json::parse(R"([{"guild_id":"g1","name":"Order","has_page":true,"published":true,"stale":true},{"name":"no id"}])"));
        REQUIRE(mine.size() == 1);
        CHECK(mine[0].Status() == "Not shown: not saved for 60 days");
    }

    TEST_CASE("reads the members and the recruiters") {
        auto list = Recruiter::ParseAll(Json::parse(R"([{"user_id":"u1","account_name":"Ada.1234","role":"leader"},{"account_name":"no id"}])"));
        REQUIRE(list.size() == 1);
        CHECK(list[0].role == "leader");
    }
}

TEST_SUITE("rules") {
    TEST_CASE("presets add up, and every preset role is one the backend knows") {
        CHECK(EventRules::Size(EventRules::Preset("Squad of 10")) == 10);
        CHECK(EventRules::Size(EventRules::Preset("Party of 5")) == 5);
        CHECK_FALSE(EventRules::Size(EventRules::Preset("No roles")).has_value());
        for (const auto& name : EventRules::PresetNames) {
            for (const auto& kv : EventRules::Preset(name)) {
                CHECK(std::find(EventRules::RoleKeys.begin(), EventRules::RoleKeys.end(), kv.first) != EventRules::RoleKeys.end());
            }
        }
    }

    TEST_CASE("names roles and activities") {
        Loc::Use("en");
        CHECK(EventRules::RoleLabel("boon_dps") == "Boon DPS");
        CHECK(EventRules::RoleLabel("bard") == "Bard");
        CHECK(EventRules::ActivityLabel("worldboss") == "World boss");
        CHECK(EventRules::DurationLabel(30) == "30 min");
        CHECK(EventRules::DurationLabel(120) == "2 h");
        CHECK(EventRules::DurationLabel(90) == "1 h 30 min");
        CHECK(EventRules::RegionLabel("eu") == "EU");
    }

    TEST_CASE("the game names a profession the way the api does") {
        CHECK(EventRules::ProfessionOrEmpty("guardian") == "Guardian");
        CHECK(EventRules::ProfessionOrEmpty("Amalgam").empty());
        CHECK(EventRules::ProfessionOfId(5) == "Thief");
        CHECK(EventRules::ProfessionOfId(9) == "Revenant");
        CHECK(EventRules::ProfessionOfId(10).empty());
    }

    TEST_CASE("a long text is cut on a letter") {
        CHECK(Shorten("short", 10) == "short");
        CHECK(Shorten("line\nbreak", 20) == "line break");
        CHECK(Shorten(u8"çççççççççç", 5) == u8"çççç…");
        CHECK(Length(u8"日本語") == 3);
    }
}

TEST_SUITE("event draft") {
    TEST_CASE("a good draft has nothing wrong") { CHECK(Good().Validate(Now).empty()); }

    TEST_CASE("the title is three to eighty") {
        Loc::Use("en");
        for (const std::string& title : {std::string("ab"), std::string(81, 'x'), std::string("   ")}) {
            EventDraft d = Good();
            d.title = title;
            CHECK(d.Validate(Now) == "The title has to be 3 to 80 characters.");
        }
    }

    TEST_CASE("the time has to be read and in the future") {
        EventDraft d = Good();
        d.whenText = "soon";
        CHECK(d.Validate(Now).find("could not read") != std::string::npos);
        d.whenText = "2026-10-09 10:00";
        CHECK(d.Validate(Now) == "The start must be in the future.");
    }

    TEST_CASE("an event that has begun can still be changed but not made in the past") {
        EventDraft d = Good();
        d.whenText = "2026-10-09 10:00";
        d.editId = "e1";
        CHECK(d.Validate(Now).empty());
    }

    TEST_CASE("places are two to a hundred when there are no roles") {
        EventDraft d = Good();
        d.maxText = "1";
        CHECK(d.Validate(Now) == "The places have to be 2 to 100.");
        d.maxText = "abc";
        CHECK_FALSE(d.Validate(Now).empty());
        d.maxText = "10";
        CHECK(d.Validate(Now).empty());
        CHECK(d.MaxParticipants() == 10);
        d.maxText = "";
        CHECK_FALSE(d.MaxParticipants().has_value());
    }

    TEST_CASE("the map is a number in range for a public event") {
        EventDraft d = Good();
        d.mapText = "0";
        CHECK_FALSE(d.Validate(Now).empty());
        d.mapText = "1234";
        CHECK(d.Validate(Now).empty());
        CHECK(d.ToOpen(Now).mapId == 1234);
    }

    TEST_CASE("roles need the role and the profession of the organizer") {
        EventDraft d = Good();
        d.UsePreset("Party of 5");
        CHECK(d.Validate(Now) == "Pick the role you come as.");
        d.myRole = "dps";
        CHECK(d.Validate(Now) == "Pick your profession.");
        d.myProfession = "Thief";
        CHECK(d.Validate(Now).empty());
        NewOpenEvent o = d.ToOpen(Now);
        CHECK_FALSE(o.maxParticipants.has_value());
        CHECK(o.myRole == "dps");
        CHECK(d.MaxParticipants() == 5);
    }

    TEST_CASE("changing the roles drops a role that is gone") {
        EventDraft d = Good();
        d.UsePreset("Party of 5");
        d.myRole = "healer";
        d.ChangeRole("healer", -1);
        CHECK(d.roles.count("healer") == 0);
        CHECK(d.myRole.empty());
        d.ChangeRole("tank", 60);
        CHECK(d.roles["tank"] == 50);
    }

    TEST_CASE("at most three tags") {
        EventDraft d = Good();
        CHECK(d.ToggleTag("meta", true));
        CHECK(d.ToggleTag("casual", true));
        CHECK(d.ToggleTag("training", true));
        CHECK_FALSE(d.ToggleTag("speedrun", true));
        CHECK(d.ToggleTag("meta", false));
        CHECK(d.ToggleTag("speedrun", true));
    }

    TEST_CASE("a guild event needs the account it is organized with") {
        EventDraft d = Good();
        d.guildId = "g1";
        CHECK(d.Validate(Now) == "Pick the game account you organize with.");
        d.accountId = "a1";
        CHECK(d.Validate(Now).empty());
    }

    TEST_CASE("a long note is refused") {
        EventDraft d = Good();
        d.description = std::string(1001, 'x');
        CHECK(d.Validate(Now) == "The notes can be 1000 characters at most.");
    }

    TEST_CASE("the form of an event that is changed begins as the event is") {
        EventInfo e;
        e.id = "e1";
        e.title = "Fractals";
        e.activity = "fractal";
        e.language = "en";
        e.region = "eu";
        e.startAt = At(2026, 10, 12, 20, 0);
        e.durationMinutes = 100;
        e.tags = {"meta"};
        e.mapId = 50;
        e.maxParticipants = 5;
        EventDraft d = EventDraft::FromEvent(e);
        CHECK(d.IsEdit());
        CHECK_FALSE(d.IsGuild());
        CHECK(d.whenText == "2026-10-12 20:00");
        CHECK(d.durationMinutes == 100);  // as the event has it, as the app keeps it
        CHECK(d.maxText == "5");
        CHECK(d.mapText == "50");
        CHECK(d.tags.count("meta") == 1);
    }

    TEST_CASE("a weekly slot is in the zone of the player, sunday is seven, and an unknown zone is UTC") {
        WeeklySlot s = WeeklySlot::Of(At(2026, 10, 11, 20, 0), "Turkey Standard Time");
        CHECK(s.weekday == 7);
        CHECK(s.zone == "Europe/Istanbul");
        WeeklySlot u = WeeklySlot::Of(At(2026, 10, 9, 20, 0), "Nowhere Standard Time");
        CHECK(u.zone == "UTC");
        CHECK(u.timeOfDay == "20:00");
    }

    TEST_CASE("the zones of Mexico are known by the names Windows gives them") {
        CHECK(TimeZones::ToIana("Central Standard Time (Mexico)") == "America/Mexico_City");
        CHECK(TimeZones::ToIana("Pacific Standard Time (Mexico)") == "America/Tijuana");
        CHECK(TimeZones::ToIana("Mexico Standard Time") == "");
    }
}

TEST_SUITE("recruit draft") {
    RecruitDraft GoodPage() {
        RecruitDraft d;
        d.headline = "Raids every week";
        d.description = "A friendly guild that raids twice a week.";
        d.contact = "Ada.1234";
        return d;
    }

    TEST_CASE("a good page can be saved and shown") {
        CHECK(GoodPage().Validate(false).empty());
        CHECK(GoodPage().Validate(true).empty());
    }

    TEST_CASE("a draft may be empty but a page that is shown may not") {
        RecruitDraft d;
        CHECK(d.Validate(false).empty());
        CHECK(d.Validate(true) == "A page that is shown needs a headline.");
        d.headline = "x";
        CHECK(d.Validate(true) == "The description needs 20 characters at least.");
        d.description = std::string(20, 'x');
        CHECK(d.Validate(true) == "Give a way to reach the guild: a Discord invite or a name to whisper.");
    }

    TEST_CASE("the invite has to be a Discord address") {
        for (const char* invite : {"https://discord.gg/abc", "https://discord.com/invite/abc-1", "https://discord.gg/abc/"}) CHECK(IsDiscordInvite(invite));
        for (const char* invite : {"discord.gg/abc", "http://discord.gg/abc", "https://evil.com/discord.gg/abc", "https://discord.gg/"}) CHECK_FALSE(IsDiscordInvite(invite));
    }

    TEST_CASE("the texts have their limits") {
        RecruitDraft d = GoodPage();
        d.headline = std::string(81, 'x');
        CHECK(d.Validate(false) == "The headline can be 80 characters at most.");
        d = GoodPage();
        d.requirements = std::string(501, 'x');
        CHECK(d.Validate(false) == "The requirements can be 500 characters at most.");
    }

    TEST_CASE("four things looked for and one language") {
        RecruitDraft d = GoodPage();
        for (const char* t : {"raids", "fractals", "strikes", "wvw"}) CHECK(d.ToggleLookingFor(t, true));
        CHECK_FALSE(d.ToggleLookingFor("pvp", true));
        d.SetLanguage("de");
        d.SetLanguage("en");
        CHECK(d.languages.size() == 1);
        CHECK(d.TagCount() == 5);
    }

    TEST_CASE("every field is sent and a cleared invite is null") {
        RecruitDraft d = GoodPage();
        d.lookingFor = {"wvw", "raids"};
        Json j = d.ToJson();
        CHECK(j["discord_invite"].is_null());
        CHECK(j["looking_for"] == Json::array({"raids", "wvw"}));
        CHECK(j["recruiting"] == true);
        CHECK(j.contains("play_times"));
    }
}

TEST_SUITE("reminders") {
    EventInfo Soon(const std::string& id, int minutes, const std::string& status = "going") {
        EventInfo e;
        e.id = id;
        e.title = "Raid night";
        e.startAt = Now + minutes * 60;
        e.myStatus = status;
        return e;
    }

    TEST_CASE("an event that begins soon is told once") {
        ReminderPlanner p;
        std::vector<EventInfo> events{Soon("e1", 5)};
        CHECK(p.Due(events, Now, 10).size() == 1);
        CHECK(p.Due(events, Now, 10).empty());
    }

    TEST_CASE("only what is within the minutes and not long gone") {
        ReminderPlanner p;
        CHECK(p.Due({Soon("far", 30), Soon("gone", -5)}, Now, 10).empty());
        CHECK(p.Due({Soon("just", -1)}, Now, 10).size() == 1);
    }

    TEST_CASE("only what the player is in or made, and not what was cancelled") {
        ReminderPlanner p;
        EventInfo other = Soon("other", 5, "");
        EventInfo made = Soon("made", 5, "");
        made.mine = true;
        EventInfo cancelled = Soon("cancelled", 5);
        cancelled.status = "cancelled";
        auto due = p.Due({other, made, cancelled}, Now, 10);
        REQUIRE(due.size() == 1);
        CHECK(due[0].id == "made");
    }

    TEST_CASE("the words of the notice") {
        Loc::Use("en");
        CHECK(ReminderPlanner::Message(Soon("e", 5), Now) == "Raid night starts in 5 minutes");
        CHECK(ReminderPlanner::Message(Soon("e", 1), Now) == "Raid night starts in 1 minute");
        CHECK(ReminderPlanner::Message(Soon("e", 0), Now) == "Raid night is starting now");
    }
}

TEST_SUITE("settings") {
    TEST_CASE("keeps what was chosen, held to its ranges") {
        TempDir dir;
        Settings s;
        s.language = "tr";
        s.reminderMinutes = 99;
        s.defaultAccountId = "a1";
        s.Clamp();
        CHECK(s.reminderMinutes == 60);
        CHECK(s.Save(dir.path));
        Settings back = Settings::Load(dir.path);
        CHECK(back.language == "tr");
        CHECK(back.defaultAccountId == "a1");
        CHECK(back.reminderMinutes == 60);
    }

    TEST_CASE("a file that is not there or not json gives the defaults") {
        TempDir dir;
        CHECK(Settings::Load(dir.path).language == "auto");
        std::ofstream(dir.path / "settings.json") << "{nope";
        CHECK(Settings::Load(dir.path).refreshMinutes == 5);
    }

    TEST_CASE("the link of a public event") { CHECK(PublicEventLink("e1") == "https://yesanith.github.io/wc-frontend/app/#/event/e1"); }
}
