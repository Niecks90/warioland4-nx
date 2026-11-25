#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "runtime.h"
#include "runtime_arm.h"
#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

#if defined(__SWITCH__)
#include <SDL.h>
#include <fstream>
#include <switch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "gba_bus.h"
#include "gba_ppu.h"
#include "host_window.h"
#include "runtime_bus_bridge.h"
#endif

namespace {

// Wario Land 4 dynamic IWRAM stack code execution:
// The cartridge writes Thumb return/pop stubs on the stack in 0x03007D0C..0x03007D90.
// Because these addresses and stack contents vary dynamically from run to run,
// they cannot and must not be declared as static [[code_copy]].
// Instead, they are interpreted directly from live IWRAM memory through
// g_runtime_force_interp_hook, eliminating any dependence on runtime GCC self-heal.
extern "C" int wario_force_interp_hook(uint32_t pc, int /*thumb*/) {
    if (pc >= 0x03007D00u && pc < 0x03007E00u) {
        return 1;
    }
    return 0;
}

}  // namespace

#if defined(__SWITCH__)
static void log_boot(const char* fmt, ...) {
    FILE* f = fopen("sdmc:/switch/warioland4_nx/boot.log", "a");
    if (f) {
        va_list args, args_copy;
        va_start(args, fmt);
        va_copy(args_copy, args);
        vfprintf(f, fmt, args);
        vprintf(fmt, args_copy);
        va_end(args_copy);
        va_end(args);
        fflush(f);
        fclose(f);
    }
}

extern "C" const uint32_t kDispatchTableLen;
extern "C" bool g_config_audio = true;
bool g_config_show_fps = false;

static inline uint16_t read_io16(const uint8_t* io, size_t off) {
    return static_cast<uint16_t>(io[off]) |
           (static_cast<uint16_t>(io[off + 1]) << 8);
}

static void wario_extended_view_frame(const gbarecomp::ExtendedViewFrameInfo* info) {
    if (g_wl4_qol.widescreen_mode == WL4_WS_CLASSIC) {
        gba::g_ws_pillarbox = 1;
        return;
    }
    if (g_wl4_qol.widescreen_mode == WL4_WS_FORCE) {
        gba::g_ws_pillarbox = 0;
        return;
    }
    if (!info || !info->io || info->io_size < 0x20) {
        gba::g_ws_pillarbox = 0;
        return;
    }
    const uint16_t dispcnt = read_io16(info->io, 0x000);
    const uint16_t bg_mode = dispcnt & 0x7u;
    const uint16_t bg_mask = (dispcnt >> 8) & 0x0Fu;
    if (bg_mode >= 3 || bg_mask == 0) {
        gba::g_ws_pillarbox = 1;
        return;
    }

    // Check gMainGameMode in IWRAM (0x03000C3A): modes 2 and 8 are active stage gameplay.
    uint16_t main_mode = 0xFFFFu;
    if (auto* bus = gbarecomp::active_bus()) {
        if (const uint8_t* iwram = bus->iwram_ptr()) {
            main_mode = static_cast<uint16_t>(iwram[0x0C3A]) |
                        (static_cast<uint16_t>(iwram[0x0C3B]) << 8);
        }
    }
    if (main_mode == 2 || main_mode == 8) {
        gba::g_ws_pillarbox = 0;
        return;
    }

    // Boot/intro/menus (mode != 2 && mode != 8): pillarbox static 256px screens
    // where every active BG has size == 0 and HOFS == 0; keep scrolling cutscenes wide.
    bool all_static_256 = true;
    for (int bg = 0; bg < 4; ++bg) {
        if (!(bg_mask & (1u << bg))) continue;
        const uint16_t bgcnt = read_io16(info->io, 0x008u + static_cast<size_t>(bg) * 2u);
        const uint16_t hofs = read_io16(info->io, 0x010u + static_cast<size_t>(bg) * 4u) & 0x1FFu;
        const uint16_t size_bits = (bgcnt >> 14) & 0x3u;
        if (size_bits != 0 || hofs != 0) {
            all_static_256 = false;
            break;
        }
    }
    gba::g_ws_pillarbox = all_static_256 ? 1 : 0;
}

static const char* wl4_filter_token(int f) {
    switch (f) {
        case WL4_FILTER_BILINEAR: return "bilinear";
        case WL4_FILTER_SHARP4X:  return "sharp4x";
        case WL4_FILTER_LCD_GRID: return "lcd";
        default:                  return "nearest";
    }
}

static const char* wl4_color_token(int c) {
    switch (c) {
        case WL4_COLOR_BACKLIT:  return "backlit";
        case WL4_COLOR_FRONTLIT: return "frontlit";
        case WL4_COLOR_UNLIT:    return "unlit";
        case WL4_COLOR_CLASSIC:  return "classic";
        default:                 return "raw";
    }
}

extern "C" void save_wl4_config(void) {
    g_config_audio = (g_wl4_qol.audio_enabled != 0);
    g_config_show_fps = (g_wl4_qol.show_fps != 0);
    const char* path = (g_wl4_qol.config_path[0] != '\0')
        ? g_wl4_qol.config_path
        : "sdmc:/switch/warioland4_nx/config.txt";
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    int ws_val = 1;
    if (g_wl4_qol.widescreen_mode == WL4_WS_CLASSIC) ws_val = 0;
    else if (g_wl4_qol.widescreen_mode == WL4_WS_FORCE) ws_val = 2;
    out << "widescreen = " << ws_val << "\n";
    out << "video_filter = " << wl4_filter_token(g_wl4_qol.video_filter) << "\n";
    out << "color_mode = " << wl4_color_token(g_wl4_qol.color_mode) << "\n";
    out << "linear_filter = " << (g_wl4_qol.video_filter == WL4_FILTER_BILINEAR ? 1 : 0) << "\n";
    out << "vsync = " << (g_wl4_qol.vsync ? 1 : 0) << "\n";
    out << "uncapped = " << (g_wl4_qol.uncapped ? 1 : 0) << "\n";
    out << "cpu_boost = " << g_wl4_qol.cpu_boost << "\n";
    out << "show_fps = " << (g_wl4_qol.show_fps ? 1 : 0) << "\n";
    out << "audio = " << (g_wl4_qol.audio_enabled ? 1 : 0) << "\n";
    out << "bios_hle = " << (g_wl4_qol.bios_hle ? 1 : 0) << "\n";
}

struct GameThreadArgs {
    int argc;
    char** argv;
    gbarecomp::RunOptions* opts;
    int return_code;
};

static void game_runner_thread(void* arg) {
    auto* ctx = static_cast<GameThreadArgs*>(arg);
    const Result affinity_rc =
        svcSetThreadCoreMask(threadGetSelf()->handle, 1, 1u << 1);
    log_boot("Game thread affinity=0x%x rc=0x%08x on Core %u\n",
             1u << 1, affinity_rc, svcGetCurrentProcessorNumber());
    try {
        log_boot("Starting gbarecomp runtime inside 32 MiB stack thread...\n");
        g_runtime_force_interp_hook = &wario_force_interp_hook;
        ctx->return_code = gbarecomp::run_game(ctx->argc, ctx->argv, *ctx->opts);
        log_boot("Exited gbarecomp runtime cleanly with code %d.\n", ctx->return_code);
    } catch (const std::exception& e) {
        log_boot("FATAL EXCEPTION in game thread: %s\n", e.what());
        ctx->return_code = 1;
    } catch (...) {
        log_boot("FATAL UNKNOWN EXCEPTION in game thread!\n");
        ctx->return_code = 1;
    }
}
#endif

int main(int argc, char** argv) {
#if defined(__SWITCH__)
    std::set_terminate([]() {
        log_boot("FATAL: std::terminate() called!\n");
    });
    atexit([]() {
        log_boot("Application exited via atexit / end.\n");
    });

    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/warioland4_nx", 0777);
    mkdir("sdmc:/switch/warioland4_nx/save", 0777);
    mkdir("sdmc:/switch/warioland4-nx", 0777);
    mkdir("sdmc:/switch/warioland4-nx/savestates", 0777);
    chdir("sdmc:/switch/warioland4_nx");
    setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 1);
    setenv("GBARECOMP_DIAGNOSTICS", "0", 1);
    // On Switch, calling present() with SDL_RENDERER_PRESENTVSYNC on the
    // emulation thread (Core 1) causes SDL_RenderPresent to block up to
    // 16.67 ms/frame waiting for scanout, starving the GBA emulation loop and
    // reducing frame rate to <= 30 FPS. Setting GBARECOMP_NO_VSYNC=1 ensures
    // SDL_RenderPresent returns immediately, pacing smoothly at 60 FPS via audio.
    setenv("GBARECOMP_NO_VSYNC", "1", 1);
    unsetenv("GBARECOMP_VSYNC");

    FILE* log_f = fopen("sdmc:/switch/warioland4_nx/boot.log", "w");
    if (log_f) {
        fprintf(log_f, "--- Wario Land 4 NX Boot Log ---\n");
        fclose(log_f);
    }

    log_boot("Verified directories.\n");

    SDL_LogSetOutputFunction([](void*, int, SDL_LogPriority priority, const char* message) {
        log_boot("[SDL] (%d) %s\n", priority, message);
    }, nullptr);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    std::string rom_path = "sdmc:/switch/warioland4_nx/rom.gba";
    FILE* rom = fopen(rom_path.c_str(), "rb");
    if (!rom) {
        rom_path = "sdmc:/switch/warioland4_nx/Wario Land 4 (USA, Europe).gba";
        rom = fopen(rom_path.c_str(), "rb");
    }
    if (rom) {
        fseek(rom, 0, SEEK_END);
        long size = ftell(rom);
        fclose(rom);
        log_boot("Found ROM (%s): %ld bytes.\n", rom_path.c_str(), size);
    } else {
        log_boot("ERROR: Missing sdmc:/switch/warioland4_nx/rom.gba!\n");
        printf("ERROR: Missing sdmc:/switch/warioland4_nx/rom.gba!\n");
        svcSleepThread(5000000000ULL);
        return 1;
    }

    FILE* bios = fopen("sdmc:/switch/warioland4_nx/gba_bios.bin", "rb");
    if (bios) {
        fseek(bios, 0, SEEK_END);
        long size = ftell(bios);
        fclose(bios);
        log_boot("Found gba_bios.bin: %ld bytes.\n", size);
    } else {
        log_boot("ERROR: Missing sdmc:/switch/warioland4_nx/gba_bios.bin!\n");
        printf("ERROR: Missing sdmc:/switch/warioland4_nx/gba_bios.bin!\n");
        svcSleepThread(5000000000ULL);
        return 1;
    }

    log_boot("kDispatchTableLen = %u\n", kDispatchTableLen);

    int config_widescreen_mode = WL4_WS_AUTO;
    int config_video_filter = WL4_FILTER_NEAREST;
    bool has_explicit_video_filter = false;
    int config_color_mode = WL4_COLOR_RAW;
    bool config_vsync = false;
    bool config_uncapped = false;
    bool config_bios_hle = true;
    int config_cpu_boost = 0;  // 0 = Normal, 1 = FastLoad, 2 = Type2
    g_config_show_fps = false;
    g_config_audio = true;

    const std::string config_paths[] = {
        "sdmc:/switch/warioland4_nx/config.txt",
        "sdmc:/switch/warioland4-nx/config.txt",
        "./config.txt"
    };

    std::string loaded_config_path;
    std::ifstream config_file;
    for (const auto& path : config_paths) {
        config_file.clear();
        config_file.open(path);
        if (config_file.is_open()) {
            loaded_config_path = path;
            break;
        }
    }

    if (config_file.is_open()) {
        log_boot("Loaded config from %s\n", loaded_config_path.c_str());
        std::string line;
        while (std::getline(config_file, line)) {
            const size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#' || line[first] == ';') continue;
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

            const auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (auto& c : val) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                const bool is_true = (val == "1" || val == "true" || val == "on" || val == "yes");
                const bool is_false = (val == "0" || val == "false" || val == "off" || val == "no");

                if (key == "widescreen") {
                    if (val == "2" || val == "force") config_widescreen_mode = WL4_WS_FORCE;
                    else if (is_true || val == "auto") config_widescreen_mode = WL4_WS_AUTO;
                    else if (is_false || val == "classic") config_widescreen_mode = WL4_WS_CLASSIC;
                } else if (key == "video_filter" || key == "videofilter" || key == "filter") {
                    has_explicit_video_filter = true;
                    if (val == "bilinear" || val == "linear" || val == "1") config_video_filter = WL4_FILTER_BILINEAR;
                    else if (val == "sharp4x" || val == "sharp" || val == "2") config_video_filter = WL4_FILTER_SHARP4X;
                    else if (val == "lcd" || val == "lcd_grid" || val == "grid" || val == "3") config_video_filter = WL4_FILTER_LCD_GRID;
                    else config_video_filter = WL4_FILTER_NEAREST;
                } else if (key == "color_mode" || key == "colormode" || key == "color") {
                    if (val == "backlit" || val == "1") config_color_mode = WL4_COLOR_BACKLIT;
                    else if (val == "frontlit" || val == "2") config_color_mode = WL4_COLOR_FRONTLIT;
                    else if (val == "unlit" || val == "3") config_color_mode = WL4_COLOR_UNLIT;
                    else if (val == "classic" || val == "4") config_color_mode = WL4_COLOR_CLASSIC;
                    else config_color_mode = WL4_COLOR_RAW;
                } else if (key == "linear_filter" || key == "linearfilter") {
                    if (!has_explicit_video_filter && is_true) config_video_filter = WL4_FILTER_BILINEAR;
                } else if (key == "show_fps" || key == "showfps") {
                    if (is_true) g_config_show_fps = true;
                    else if (is_false) g_config_show_fps = false;
                } else if (key == "vsync") {
                    if (is_true) config_vsync = true;
                    else if (is_false) config_vsync = false;
                } else if (key == "uncapped") {
                    if (is_true) config_uncapped = true;
                    else if (is_false) config_uncapped = false;
                } else if (key == "cpu_boost" || key == "cpuboost") {
                    config_cpu_boost = std::clamp(std::atoi(val.c_str()), 0, 2);
                } else if (key == "audio") {
                    if (is_true) g_config_audio = true;
                    else if (is_false) g_config_audio = false;
                } else if (key == "bios_hle" || key == "bioshle") {
                    if (is_true) config_bios_hle = true;
                    else if (is_false) config_bios_hle = false;
                }
            }
        }
    } else {
        loaded_config_path = "sdmc:/switch/warioland4_nx/config.txt";
        log_boot("Config not found, creating default at %s\n", loaded_config_path.c_str());
    }

    g_wl4_qol.widescreen_mode = config_widescreen_mode;
    g_wl4_qol.video_filter = config_video_filter;
    g_wl4_qol.color_mode = config_color_mode;
    g_wl4_qol.show_fps = g_config_show_fps ? 1 : 0;
    g_wl4_qol.vsync = config_vsync ? 1 : 0;
    g_wl4_qol.uncapped = config_uncapped ? 1 : 0;
    g_wl4_qol.cpu_boost = config_cpu_boost;
    g_wl4_qol.audio_enabled = g_config_audio ? 1 : 0;
    g_wl4_qol.bios_hle = config_bios_hle ? 1 : 0;
    g_wl4_qol.reset_requested = 0;
    std::snprintf(g_wl4_qol.config_path, sizeof(g_wl4_qol.config_path), "%s",
                  loaded_config_path.empty() ? "sdmc:/switch/warioland4_nx/config.txt" : loaded_config_path.c_str());
    std::snprintf(g_wl4_qol.savestate_dir, sizeof(g_wl4_qol.savestate_dir),
                  "sdmc:/switch/warioland4-nx/savestates");
    g_wl4_qol.save_config_fn = &save_wl4_config;
    if (!config_file.is_open()) {
        save_wl4_config();
    }

    ApmCpuBoostMode boost_mode = ApmCpuBoostMode_Normal;
    if (config_cpu_boost == 1) boost_mode = ApmCpuBoostMode_FastLoad;
    else if (config_cpu_boost == 2) boost_mode = ApmCpuBoostMode_Type2;
    const Result boost_rc = appletSetCpuBoostMode(boost_mode);
    log_boot("CPU boost mode %d rc=0x%08x\n", config_cpu_boost, boost_rc);

    if (config_vsync) {
        setenv("GBARECOMP_VSYNC", "1", 1);
        unsetenv("GBARECOMP_NO_VSYNC");
    } else {
        setenv("GBARECOMP_NO_VSYNC", "1", 1);
        unsetenv("GBARECOMP_VSYNC");
    }
    setenv("GBARECOMP_BIOS_HLE", config_bios_hle ? "1" : "0", 1);

    log_boot("Config: ws=%d, filter=%d, color=%d, vsync=%d, uncapped=%d, cpu_boost=%d, show_fps=%d, audio=%d, bios_hle=%d\n",
             g_wl4_qol.widescreen_mode, g_wl4_qol.video_filter, g_wl4_qol.color_mode,
             g_wl4_qol.vsync, g_wl4_qol.uncapped, g_wl4_qol.cpu_boost,
             g_wl4_qol.show_fps, g_wl4_qol.audio_enabled, g_wl4_qol.bios_hle);
#endif

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("WarioLand4Recomp v%s\n", WL4_VERSION);
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            std::printf(
                "WarioLand4Recomp [--bios <path>] [--rom <path>] [--view-width <240..284>] [game.toml]\n");
            return 0;
        }
    }

    gbarecomp::RunOptions opts;
    std::vector<std::string> args(argv, argv + argc);

    opts.builtin_game_name = "Wario Land 4";
    opts.builtin_rom_sha1 = "b9fe05a8080e124b67bce6a623234ee3b518a2c1";
    opts.builtin_rom_crc32 = 0xD6141609u;
    opts.freely_resizable_window = true;
    opts.expose_assist_tools = false;
    opts.resize_driven_view = false;
    opts.max_resize_view_width = 240;
    opts.widescreen_view_width = 284;
    opts.launcher_region = "USA/Europe";
    opts.launcher_expose_widescreen = true;
    opts.launcher_expose_adaptive_view = false;

#if defined(__SWITCH__)
    opts.launcher_save_path = "sdmc:/switch/warioland4_nx/save/wario_land_4_awae.sav";
    opts.max_view_width = 284;
    opts.extended_view_frame = &wario_extended_view_frame;
    opts.show_fps_by_default = (g_wl4_qol.show_fps != 0);

    static char arg_prog[]          = "warioland4_nx";
    static char arg_bios_flag[]     = "--bios";
    static char arg_bios_path[]     = "sdmc:/switch/warioland4_nx/gba_bios.bin";
    static char arg_rom_flag[]      = "--rom";
    static char arg_save_flag[]     = "--save";
    static char arg_save_path[]     = "sdmc:/switch/warioland4_nx/save/wario_land_4_awae.sav";
    static char arg_state_flag[]    = "--state-dir";
    static char arg_state_dir[]     = "sdmc:/switch/warioland4-nx/savestates";
    static char arg_view_flag[]     = "--view-width";
    static char arg_view_wide[]     = "284";
    static char arg_linear_flag[]   = "--linear-filter";
    static char arg_uncapped_flag[] = "--uncapped";
    static char arg_hle[]           = "--bios-hle";

    std::vector<char> rom_arg_buf(rom_path.begin(), rom_path.end());
    rom_arg_buf.push_back('\0');

    int last_return_code = 0;
    do {
        g_wl4_qol.reset_requested = 0;
        g_config_audio = (g_wl4_qol.audio_enabled != 0);
        g_config_show_fps = (g_wl4_qol.show_fps != 0);
        opts.show_fps_by_default = g_config_show_fps;
        if (g_wl4_qol.vsync) {
            setenv("GBARECOMP_VSYNC", "1", 1);
            unsetenv("GBARECOMP_NO_VSYNC");
        } else {
            setenv("GBARECOMP_NO_VSYNC", "1", 1);
            unsetenv("GBARECOMP_VSYNC");
        }
        setenv("GBARECOMP_BIOS_HLE", g_wl4_qol.bios_hle ? "1" : "0", 1);

        std::vector<char*> av;
        av.push_back(argv && argv[0] ? argv[0] : arg_prog);
        av.push_back(arg_bios_flag);
        av.push_back(arg_bios_path);
        av.push_back(arg_rom_flag);
        av.push_back(rom_arg_buf.data());
        av.push_back(arg_save_flag);
        av.push_back(arg_save_path);
        av.push_back(arg_state_flag);
        av.push_back(arg_state_dir);
        av.push_back(arg_view_flag);
        av.push_back(arg_view_wide);
        if (g_wl4_qol.bios_hle) {
            av.push_back(arg_hle);
        }
        if (g_wl4_qol.video_filter == WL4_FILTER_BILINEAR) {
            av.push_back(arg_linear_flag);
        }
        if (g_wl4_qol.uncapped) {
            av.push_back(arg_uncapped_flag);
        }
        av.push_back(nullptr);

        GameThreadArgs thread_args;
        thread_args.argc = static_cast<int>(av.size() - 1);
        thread_args.argv = av.data();
        thread_args.opts = &opts;
        thread_args.return_code = 0;

        Thread game_thread;
        constexpr size_t kStackSize = 32 * 1024 * 1024;  // 32 MiB stack
        Result rc = threadCreate(&game_thread, game_runner_thread, &thread_args, nullptr, kStackSize, 0x2C, 1);
        if (R_FAILED(rc)) {
            log_boot("ERROR: Failed to create game thread (rc=0x%x)!\n", rc);
            return 1;
        }

        log_boot("Launching game thread...\n");
        rc = threadStart(&game_thread);
        if (R_FAILED(rc)) {
            log_boot("ERROR: Failed to start game thread (rc=0x%x)!\n", rc);
            threadClose(&game_thread);
            return 1;
        }

        threadWaitForExit(&game_thread);
        threadClose(&game_thread);
        last_return_code = thread_args.return_code;
        if (g_wl4_qol.reset_requested) {
            log_boot("Reset requested from QoL menu; rebooting runtime...\n");
        }
    } while (g_wl4_qol.reset_requested != 0);

    return last_return_code;
#else
    opts.max_view_width = 284;
    opts.launcher_game_config = "game.toml";
    opts.launcher_save_path = "saves/wario_land_4_awae.sav";

#if defined(GBAGAME_RECOMP_UI)
    if (game_launcher_preboot(args, opts)) return 0;
#endif

    g_runtime_force_interp_hook = &wario_force_interp_hook;
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#endif
}
