cmake_minimum_required(VERSION 3.20)
project(ziggurat_port CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Keep IEEE float semantics identical to the original (no fast-math).

# Test hooks (ZIGGURAT_* env vars, launcher dev screens/flags): ON for development and CI test
# builds, OFF for release packages (tools/package.py). See docs/TESTING.md.
option(ZIGGURAT_TEST_HOOKS "Compile test hooks and developer-only launcher screens" ON)

# SDL3: system package by default (MSYS2 / distro); ZIGGURAT_FETCH_SDL3=ON builds a pinned release
# from source instead (reproducible CI and Linux builds).
option(ZIGGURAT_FETCH_SDL3 "Fetch and build pinned SDL3 instead of find_package" OFF)
set(ZIGGURAT_SDL3_TAG "release-3.2.20" CACHE STRING "SDL3 git tag used when ZIGGURAT_FETCH_SDL3=ON")
if (ZIGGURAT_FETCH_SDL3)
  include(FetchContent)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  if (WIN32)
    # MinGW has no iconv in its C runtime: SDL would link libiconv-2.dll. Use SDL's built-in one.
    set(SDL_SYSTEM_ICONV OFF CACHE BOOL "" FORCE)
  endif()
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ${ZIGGURAT_SDL3_TAG}
    GIT_SHALLOW TRUE)
  # place the SDL3 shared library next to the executables
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  FetchContent_MakeAvailable(SDL3)
else()
  find_package(SDL3 REQUIRED CONFIG)
endif()

# ---- zigcore: config, paths, logging, dump import, steam (shared by game + launcher)
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
# sqlite3 amalgamation (public domain): reads the original iOS HighScore.sqlite in the stats importer
set(SQLITE_SOURCES third_party/sqlite/sqlite3.c)
set_source_files_properties(${SQLITE_SOURCES} PROPERTIES LANGUAGE C
  COMPILE_DEFINITIONS "SQLITE_THREADSAFE=0;SQLITE_OMIT_LOAD_EXTENSION;SQLITE_DEFAULT_MEMSTATUS=0;SQLITE_OMIT_DEPRECATED"
  COMPILE_OPTIONS "-w")
add_library(zigcore STATIC ${CORE_SOURCES} ${SQLITE_SOURCES})
target_include_directories(zigcore PUBLIC src third_party)
target_compile_definitions(zigcore PUBLIC ZIGGURAT_TEST_HOOKS=$<IF:$<BOOL:${ZIGGURAT_TEST_HOOKS}>,1,0>)
target_link_libraries(zigcore PUBLIC SDL3::SDL3)
# launcher worker threads (dump import, game process wait); needed explicitly on Linux
find_package(Threads REQUIRED)
target_link_libraries(zigcore PUBLIC Threads::Threads)
if (UNIX AND NOT APPLE)
  target_link_libraries(zigcore PUBLIC m)
endif()
if (WIN32)
  # registry (Steam path), process list, IShellLink (.lnk shortcuts), known folders
  target_link_libraries(zigcore PUBLIC advapi32 ole32 shell32 uuid winhttp)
endif()

# ---- engine + platform + game logic (shared by game + launcher theme rendering)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp src/platform/*.cpp)
add_library(zigengine STATIC ${ENGINE_SOURCES})
target_link_libraries(zigengine PUBLIC zigcore)

# ---- the game
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp)
if (WIN32)
  enable_language(RC)
  set(APP_ICON_RC ${CMAKE_SOURCE_DIR}/res/ziggurat.rc)  # original "Z" icon (tools/make_icon.py)
endif()
add_executable(ziggurat src/main.cpp ${GAME_SOURCES} ${APP_ICON_RC})
if (WIN32)
  # GUI subsystem: no console window next to the game (logs go to <data>/logs/game-*.log)
  set_target_properties(ziggurat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(ziggurat PRIVATE zigengine)

# ---- the launcher (src/launcher/main.cpp must define main)
file(GLOB_RECURSE LAUNCHER_SOURCES CONFIGURE_DEPENDS src/launcher/*.cpp)
if (LAUNCHER_SOURCES)
  add_executable(ZigguratLauncher ${LAUNCHER_SOURCES} ${APP_ICON_RC})
  target_link_libraries(ZigguratLauncher PRIVATE zigengine)
  if (WIN32)
    set_target_properties(ZigguratLauncher PROPERTIES WIN32_EXECUTABLE TRUE)
  endif()
endif()

# ---- zigimport: CLI for dump scan/verify/import (tests, manual use)
add_executable(zigimport src/tools/zigimport.cpp)
target_link_libraries(zigimport PRIVATE zigcore)

# ---- zigextras: tests for Steam Input mode, update check, reference mode (sandbox-only writes)
add_executable(zigextras src/tools/zigextras.cpp)
target_link_libraries(zigextras PRIVATE zigcore)

# ---- zigsteam: Steam integration tests (sandbox-only writes)
add_executable(zigsteam src/tools/zigsteam.cpp)
target_link_libraries(zigsteam PRIVATE zigcore)

# ---- zigstats: records export/import/backup CLI (L7)
add_executable(zigstats src/tools/zigstats.cpp)
target_link_libraries(zigstats PRIVATE zigcore)

# ---- ziginputtest: unit tests for the controller -> touch mapping (InputMap)
add_executable(ziginputtest src/tools/ziginputtest.cpp)
target_link_libraries(ziginputtest PRIVATE zigcore)

if (MINGW)
  foreach(t ziggurat ZigguratLauncher zigimport ziginputtest zigsteam zigstats zigextras)
    if (TARGET ${t})
      target_link_options(${t} PRIVATE -static-libgcc -static-libstdc++)
    endif()
  endforeach()
endif()

# ---- runtime files next to the launcher: fallback OFL font + launcher strings
if (TARGET ZigguratLauncher)
  add_custom_command(TARGET ZigguratLauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ZigguratLauncher>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/third_party/fonts/Silkscreen-Regular.ttf
            ${CMAKE_SOURCE_DIR}/third_party/fonts/OFL.txt $<TARGET_FILE_DIR:ZigguratLauncher>/fonts/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/launcher/strings
            $<TARGET_FILE_DIR:ZigguratLauncher>/strings
    VERBATIM)
endif()
