cmake_minimum_required(VERSION 3.20)

# the version of a release is given when it is built (Build in the Actions tab); a build by hand is 0.0.0
set(WC_VERSION "0.0.0" CACHE STRING "the version of the addon, as Nexus shows it")
project(WondrousCompass VERSION ${WC_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WC_BUILD_ADDON "Build the Nexus addon (Windows only)" ${WIN32})
option(WC_BUILD_TESTS "Build the tests of the logic that runs without the game" ON)

if(MSVC)
    # the source is UTF-8: the texts of nine languages are in it
    add_compile_options(/utf-8 /W4 /permissive- /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---- the logic: what the server is asked, what it answers, the forms, the times, the words. it builds anywhere, so it is tested anywhere

add_library(wc_core STATIC
    src/core/Api.cpp
    src/core/Drafts.cpp
    src/core/Models.cpp
    src/core/Rules.cpp
    src/core/Support.cpp
    src/core/Text.cpp
    src/core/Time.cpp
)
target_include_directories(wc_core PUBLIC src third_party)
if(WIN32)
    target_compile_definitions(wc_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ---- the addon: a dll Nexus loads, which draws with the ImGui of Nexus

if(WC_BUILD_ADDON)
    if(NOT WIN32)
        message(FATAL_ERROR "the addon is a Windows dll: build it with MSVC, or with MinGW through cmake/mingw.cmake")
    endif()
    add_library(WondrousCompass SHARED
        src/addon/Entry.cpp
        src/addon/Host.cpp
        src/addon/WinHttp.cpp
        src/addon/Worker.cpp
        src/addon/Widgets.cpp
        src/addon/MainWindow.cpp
        src/addon/EventPages.cpp
        src/addon/CreatePage.cpp
        src/addon/RecruitPages.cpp
        src/addon/AccountPage.cpp
        src/addon/Resources.rc
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/imgui_stdlib.cpp
    )
    target_include_directories(WondrousCompass PRIVATE src third_party third_party/imgui)
    target_link_libraries(WondrousCompass PRIVATE wc_core winhttp)
    set_target_properties(WondrousCompass PROPERTIES PREFIX "" OUTPUT_NAME "WondrousCompass")
    target_compile_definitions(WondrousCompass PRIVATE WC_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} WC_VERSION_MINOR=${PROJECT_VERSION_MINOR}
                                                       WC_VERSION_PATCH=${PROJECT_VERSION_PATCH})
    if(MSVC)
        # the C runtime is in the dll, so a player needs nothing else installed
        set_property(TARGET WondrousCompass PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET wc_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # ImGui is the code of somebody else: its warnings are not ours to fix
        set_source_files_properties(third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
                                    third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_stdlib.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    else()
        target_link_options(WondrousCompass PRIVATE -static -static-libgcc -static-libstdc++)
        set_source_files_properties(third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
                                    third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_stdlib.cpp PROPERTIES COMPILE_OPTIONS "-w")
    endif()
endif()

# ---- the tests

if(WC_BUILD_TESTS)
    enable_testing()
    add_executable(wc_tests tests/Main.cpp tests/CoreTests.cpp tests/TextTests.cpp)
    target_link_libraries(wc_tests PRIVATE wc_core)
    target_compile_definitions(wc_tests PRIVATE WC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        set_property(TARGET wc_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME wc_tests COMMAND wc_tests)
endif()

# ---- the window of the addon, drawn off the game with a pretend server: ImGui's own checks catch a Begin without its End, and the frames are
# drawn into tests/ui/out to be looked at

if(WC_BUILD_TESTS AND NOT WIN32)
    add_executable(wc_ui_tests
        tests/ui/UiTests.cpp
        tests/ui/HostStub.cpp
        src/addon/Worker.cpp
        src/addon/Widgets.cpp
        src/addon/MainWindow.cpp
        src/addon/EventPages.cpp
        src/addon/CreatePage.cpp
        src/addon/RecruitPages.cpp
        src/addon/AccountPage.cpp
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/imgui_stdlib.cpp
    )
    target_include_directories(wc_ui_tests PRIVATE src third_party third_party/imgui)
    target_link_libraries(wc_ui_tests PRIVATE wc_core pthread)
    # an assertion of ImGui is a failure of the test, not the end of the program
    target_compile_definitions(wc_ui_tests PRIVATE WC_UI_TESTS WC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
                               IMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/imconfig_tests.h")
    set_source_files_properties(third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
                                third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_stdlib.cpp TARGET_DIRECTORY wc_ui_tests PROPERTIES COMPILE_OPTIONS "-w")
    add_test(NAME wc_ui_tests COMMAND wc_ui_tests)
endif()
