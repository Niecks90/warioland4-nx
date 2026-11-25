cmake_minimum_required(VERSION 3.20)
project(WarioLand4Recomp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GBARECOMP_NETPLAY "Build desktop link-cable netplay" OFF)

set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to this game's gbarecomp worktree")
set(RECOMP_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui"
    CACHE PATH "Path to the shared recomp-ui checkout")
set(GBARECOMP_RUNTIME_UI_ROOT "${RECOMP_UI_ROOT}"
    CACHE PATH "recomp-ui root used by the live GBA runtime adapter")
if(CMAKE_SYSTEM_NAME STREQUAL "Switch" OR __SWITCH__ OR SWITCH)
    set(GBARECOMP_RUNTIME_UI_ROOT "" CACHE PATH "" FORCE)
    set(RECOMP_UI_ROOT "" CACHE PATH "" FORCE)
endif()

set(GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT OFF CACHE BOOL
    "Disable runtime self-heal overlay compilation by default" FORCE)
set(GBARECOMP_ENABLE_MODS OFF CACHE BOOL
    "Disable unused GBA mod catalog" FORCE)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/CMakeLists.txt")
    message(STATUS "gbarecomp checkout not found. Cloning gbarecomp recursively...")
    execute_process(
        COMMAND git clone --depth 1 --recurse-submodules https://github.com/mstan/gbarecomp.git "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
        RESULT_VARIABLE GBARECOMP_CLONE_RES
    )
    if(NOT GBARECOMP_CLONE_RES EQUAL 0)
        message(FATAL_ERROR "Failed to clone gbarecomp repository")
    endif()
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/external/arm-recomp-core/cmake/ArmRecompCore.cmake")
    message(STATUS "Updating gbarecomp submodules...")
    execute_process(
        COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp" submodule update --init --depth 1 --recursive
    )
endif()

# Inject Nintendo Switch host & PPU overrides into gbarecomp
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/switch/host_window.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/src/runtime/host_window.cpp"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/switch/host_window.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/src/runtime/host_window.h"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/switch/overlay_compile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/src/runtime/overlay_compile.cpp"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/switch/gba_ppu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/src/gba/gba_ppu.cpp"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/switch/host_platform.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp/src/runtime/host_platform.cpp"
    COPYONLY
)

if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "GBARECOMP_ROOT is not a gbarecomp checkout:\n"
        "  ${GBARECOMP_ROOT}\n"
        "Configure with -DGBARECOMP_ROOT=<path> if the local worktree link is absent.")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Switch" OR __SWITCH__ OR SWITCH)
    remove_definitions(-DGBARECOMP_RUNTIME_UI)
    set(GBARECOMP_RUNTIME_UI_ROOT "" CACHE PATH "" FORCE)
    set(GBARECOMP_RUNTIME_UI OFF CACHE BOOL "" FORCE)
    set(RECOMP_UI_ROOT "" CACHE PATH "" FORCE)
    add_compile_definitions(SWITCH)
    add_definitions(-U GBARECOMP_RUNTIME_UI)
endif()
set(GBARECOMP_NETPLAY OFF)
set(GBAGAME_RECOMP_UI OFF)

set(GBARECOMP_SCANLINE_IMPLEMENTATION "HLE" CACHE STRING "" FORCE)
add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)

set(GBA_GAME_GENERATED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated" CACHE PATH
    "Generated native corpus for Wario Land 4")
if(EXISTS "${GBA_GAME_GENERATED_ROOT}/recompiled.cpp")
    message(FATAL_ERROR
        "Stale monolithic generated/recompiled.cpp found. "
        "Run ./generator.sh to populate sharded generated/*.cpp files.")
endif()
file(GLOB GAME_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${GBA_GAME_GENERATED_ROOT}/*.c"
    "${GBA_GAME_GENERATED_ROOT}/*.cpp")

add_executable(WarioLand4Recomp
    src/main.cpp
)

if(GAME_GENERATED_SOURCES)
    message(STATUS "WarioLand4Recomp: building with generated game sources")
    target_sources(WarioLand4Recomp PRIVATE ${GAME_GENERATED_SOURCES})
else()
    message(WARNING "WarioLand4Recomp: generated/ is empty; run ./generator.sh with the Wario Land 4 (USA, Europe) ROM before linking.")
endif()

# Keep every generated guest shard cheaper to compile than the runtime. The
# dispatch table and symbol map remain ordinary small generated units.
set(GAME_RECOMPILED_SOURCES ${GAME_GENERATED_SOURCES})
list(FILTER GAME_RECOMPILED_SOURCES INCLUDE REGEX
    "/recompiled_[0-9]+\\.cpp$")
set(WL4_GENERATED_OPTIMIZATION "O1" CACHE STRING "Generated CPU optimization: O1 or O2")
set_property(CACHE WL4_GENERATED_OPTIMIZATION PROPERTY STRINGS O1 O2)
if(NOT WL4_GENERATED_OPTIMIZATION MATCHES "^(O1|O2)$")
    message(FATAL_ERROR "WL4_GENERATED_OPTIMIZATION must be O1 or O2")
endif()
if(GAME_RECOMPILED_SOURCES)
    set_source_files_properties(${GAME_RECOMPILED_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-${WL4_GENERATED_OPTIMIZATION};-g0")
endif()

target_include_directories(WarioLand4Recomp PRIVATE
    "${GBA_GAME_GENERATED_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug")

if(NINTENDO_SWITCH OR __SWITCH__ OR SWITCH)
    include_directories(
        /opt/devkitpro/portlibs/switch/include
        /opt/devkitpro/portlibs/switch/include/SDL2
        $ENV{DEVKITPRO}/portlibs/switch/include
        $ENV{DEVKITPRO}/portlibs/switch/include/SDL2
    )
endif()

target_link_libraries(WarioLand4Recomp PRIVATE
    "-Wl,--start-group"
    gbarecomp_runtime
    gbarecomp_debug
    gbarecomp_gba
    gbarecomp_armv4t
    "-Wl,--end-group")

if(NINTENDO_SWITCH OR __SWITCH__ OR SWITCH)
    target_compile_definitions(WarioLand4Recomp PRIVATE SWITCH)
    set_target_properties(WarioLand4Recomp PROPERTIES OUTPUT_NAME "warioland4_nx")
    target_link_directories(WarioLand4Recomp PRIVATE
        /opt/devkitpro/portlibs/switch/lib
        /opt/devkitpro/libnx/lib
    )

    target_link_options(WarioLand4Recomp PRIVATE
        "-L/opt/devkitpro/portlibs/switch/lib"
        "-L/opt/devkitpro/libnx/lib"
    )

    target_link_libraries(WarioLand4Recomp PRIVATE
        /opt/devkitpro/portlibs/switch/lib/libSDL2.a
        /opt/devkitpro/portlibs/switch/lib/libEGL.a
        /opt/devkitpro/portlibs/switch/lib/libGLESv2.a
        /opt/devkitpro/portlibs/switch/lib/libglapi.a
        /opt/devkitpro/portlibs/switch/lib/libdrm_nouveau.a
        /opt/devkitpro/libnx/lib/libnx.a
        m
    )
    target_link_libraries(WarioLand4Recomp PRIVATE gbarecomp_runtime gbarecomp_armv4t)

    add_custom_command(TARGET WarioLand4Recomp POST_BUILD
        COMMAND nacptool --create "Wario Land 4 Recomp" "Niecks90" "${PROJECT_VERSION}" $<TARGET_FILE_DIR:WarioLand4Recomp>/warioland4_nx.nacp
        COMMAND elf2nro $<TARGET_FILE:WarioLand4Recomp> $<TARGET_FILE_DIR:WarioLand4Recomp>/warioland4-nx.nro --nacp=$<TARGET_FILE_DIR:WarioLand4Recomp>/warioland4_nx.nacp --icon=${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.png
        COMMENT "Generating warioland4-nx.nro"
    )
endif()

target_compile_definitions(WarioLand4Recomp PRIVATE
    WL4_VERSION="${PROJECT_VERSION}"
    GBARECOMP_DEFAULT_DEBUG_PORT=19872
    GBARECOMP_WINDOW_TITLE="Wario Land 4"
    GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")

option(GBAGAME_RECOMP_UI "Build the recomp-ui pre-boot launcher" OFF)
if(GBAGAME_RECOMP_UI AND EXISTS "${RECOMP_UI_ROOT}/recomp_ui.cmake")
    include(${RECOMP_UI_ROOT}/recomp_ui.cmake)
    add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
    target_include_directories(game_launcher_ui PRIVATE
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/gba"
        src)
    recomp_target_launcher_ui(game_launcher_ui CONSOLE gba)
    find_package(SDL2 REQUIRED)
    target_link_libraries(game_launcher_ui PUBLIC SDL2::SDL2)
    if(WIN32)
        target_link_libraries(game_launcher_ui PUBLIC opengl32)
    endif()
    target_link_libraries(WarioLand4Recomp PRIVATE game_launcher_ui)
    set_property(SOURCE src/main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS GBAGAME_RECOMP_UI)
endif()

if(MSVC)
    target_link_options(WarioLand4Recomp PRIVATE /STACK:16777216)
elseif(MINGW OR WIN32)
    target_link_options(WarioLand4Recomp PRIVATE -Wl,--stack,16777216)
endif()
