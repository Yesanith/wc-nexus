# Wondrous Compass for Nexus

The public and guild events of [Wondrous Compass](https://yesanith.github.io/wc-frontend/) inside Guild Wars 2, as an addon of
[Nexus](https://raidcore.gg/Nexus). It does in the game what the Blish HUD module does: browse events, sign up, make and change them, find a
guild that is recruiting and write the page of your own.

> Wondrous Compass is a fan project. It is not made, endorsed or supported by ArenaNet or NCSOFT.

## Install

1. Install [Nexus](https://raidcore.gg/Nexus) if you do not have it.
2. Get `WondrousCompass.dll` from the [latest release](https://github.com/Yesanith/wc-nexus/releases/latest).
3. Put it in `Guild Wars 2\addons\` and start the game. Nexus lists it as **Wondrous Compass** in its addon library; load it there.
4. Nexus keeps it up to date from the releases of this repository.

The releases of this repository are the only official builds of the addon, see [TRADEMARKS.md](TRADEMARKS.md).

The compass icon in the bar of Nexus opens the window. A key for it can be set in the keybinds of Nexus (**Wondrous Compass**), and Escape
closes it.

## Link it to your account

The addon cannot sign in with Discord, so you link it once with a code:

1. In the Wondrous Compass app or website, sign in and open **Settings > Blish HUD · Nexus**, then press **Get a code**. The same code links the
   Blish HUD module and this addon.
2. In the game, open the window, go to **Account**, type the code and press **Link**.

A code works once and for ten minutes. The token the addon gets is kept in `Guild Wars 2\addons\WondrousCompass\link.token`. Cut a link any
time from the same Settings panel of the app, or with **Unlink** in the addon. A token can only do what the addon does (read events, sign
up, make and cancel events, write recruitment pages); it cannot reach the rest of the account. Guild events can be made by leaders and
officers only, as in the app.

## What it does

- **Public** and **Guild**: the events coming up, with the filters of the website (what it is, language, region, tag for public events;
  what it is and the guild for guild events). A card shows the time, the length, how many are going, maybe or waiting, the roles and how
  full they are. **Sign up** in the role you pick, with the profession of the character you are playing, say **Maybe**, or **Leave**.
- **My events**: the ones you are in or made.
- Click a card for the page of the event: all its notes and who is in, with the spare roles they could also play. For a guild event you
  name your own spare roles there. **Copy link** gives the address of a public event on the website, **Edit** changes an event you made
  or that your guild rank lets you change, **Cancel event** cancels one you made, and somebody else's public event can be reported.
- **Create**: a public event, or a guild event for a guild where you are a leader or an officer, once or every week. The time is read as
  you type it: `20:00`, `+1d 20:00`, `tomorrow 21:30`, `2026-12-24 19:00`, `10-12 20:00` (your local time). Roles come from a set (a squad
  of ten, a party of five) or one by one.
- **Find guild**: the guilds that are recruiting, with the filters of the website (name, language, region, what they look for, recruiting
  now). Open one to read its page and copy the name to whisper or the Discord invite. A page can be reported.
- **Recruit**: for the leader of a guild and the recruiters the leader named. Write the page, save it as a draft, show it to everybody, or
  take it back. The leader also says which members may write the page, and can delete it.
- **Reminders**: a notice on the screen a number of minutes before an event you signed up for, or made, begins.

## Settings

In the options of the addon in Nexus: the language, the reminders and how many minutes before they come, how often the events are read
again, and the size of the window. The language and the size are also on the **Account** page of the window.

## Languages

English, German, Spanish, French, Portuguese, Turkish, Russian, Japanese and Chinese. **Automatic** follows the language of Nexus, which
follows the one of the game. The window draws with Segoe UI and, for Chinese and Japanese, Microsoft YaHei, the fonts of Windows; the addon
gives Nexus every text it has, so that the fonts Nexus builds have their letters.

The words are in `src/core/lang/<code>.json`, each English text with what it says in that language. After changing one, run
`python3 tools/build_lang.py`; the check of each push fails when `src/core/lang/Tables.inc` is not the one the tables make, when the code
asks for a text a language has no word for, when a language has a word nobody asks for, or when a translation loses a `{0}` or a line
break.

## Build

The addon is a 64-bit Windows dll built with CMake:

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

writes `build/Release/WondrousCompass.dll`. MSVC builds it with the C runtime inside, so a player needs nothing else installed.

On Linux the logic and the window are tested without the game:

```
cmake -S . -B build -DWC_BUILD_ADDON=OFF
cmake --build build
./build/wc_tests       # the server calls, the forms, the times, the reminders, the words of every language
./build/wc_ui_tests    # every page of the window, drawn with a pretend server; the frames go to tests/ui/out
```

and `cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw.cmake -DWC_BUILD_TESTS=OFF` builds the dll with MinGW, to check that it compiles.

## Releases

Built by hand: run **Build** from the Actions tab, with the version and whether to publish it. Ticked, the workflow tests, builds and makes
the `v` tag and the release with `WondrousCompass.dll`, which Nexus offers as the update. Nothing is kept as an artifact.

## Project layout

```
src/core/     the logic, which builds anywhere: the server calls, what the server answers, the forms, the times, the words
src/addon/    the Nexus addon: the entry, the host (paths, fonts, the language of Nexus, the character), WinHTTP, the worker, the window
tests/        the tests of the logic, and tests/ui the window drawn off the game
tools/        the translation tables into C++, and the icons
third_party/  ImGui 1.80 (the version of Nexus), the Nexus API header, nlohmann/json and doctest
```

The calls to the server run one after another on a thread of their own, and what they bring back is handed to the frame that draws, so the
game never waits for the server.

## Backend

The addon calls the `module_*` functions of the Wondrous Compass backend with its public key, as the Blish HUD module does.

## License

MIT, see [LICENSE](LICENSE). The name Wondrous Compass, its logo and its icons are not under it: a fork is published under a
name and with art of its own, and with a signature of its own in `GetAddonDef`, see [TRADEMARKS.md](TRADEMARKS.md). The code in `third_party` is under its own licenses (MIT), in the files next to it. Guild Wars 2 and its
names and art are ArenaNet's.
