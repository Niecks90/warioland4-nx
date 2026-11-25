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

    bool config_widescreen = true;
    bool config_linear_filter = false;
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
                    if (is_true) config_widescreen = true;
                    else if (is_false) config_widescreen = false;
                } else if (key == "linear_filter" || key == "linearfilter") {
                    if (is_true) config_linear_filter = true;
                    else if (is_false) config_linear_filter = false;
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
                    config_cpu_boost = std::atoi(val.c_str());
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
        log_boot("Config not found, creating default at sdmc:/switch/warioland4_nx/config.txt\n");
        std::ofstream new_config("sdmc:/switch/warioland4_nx/config.txt");
        new_config << "widescreen = 1\n";
        new_config << "vsync = 0\n";
        new_config << "uncapped = 0\n";
        new_config << "cpu_boost = 0\n";
        new_config << "linear_filter = 0\n";
        new_config << "show_fps = 0\n";
        new_config << "audio = 1\n";
        new_config << "bios_hle = 1\n";
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

    log_boot("Config: widescreen=%d, vsync=%d, uncapped=%d, cpu_boost=%d, linear_filter=%d, show_fps=%d, audio=%d, bios_hle=%d\n",
             config_widescreen, config_vsync, config_uncapped, config_cpu_boost,
             config_linear_filter, g_config_show_fps, g_config_audio, config_bios_hle);
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
    opts.max_view_width = config_widescreen ? 284 : 240;
    opts.show_fps_by_default = g_config_show_fps;

    static char arg_prog[]          = "warioland4_nx";
    static char arg_bios_flag[]     = "--bios";
    static char arg_bios_path[]     = "sdmc:/switch/warioland4_nx/gba_bios.bin";
    static char arg_rom_flag[]      = "--rom";
    static char arg_save_flag[]     = "--save";
    static char arg_save_path[]     = "sdmc:/switch/warioland4_nx/save/wario_land_4_awae.sav";
    static char arg_view_flag[]     = "--view-width";
    static char arg_view_wide[]     = "284";
    static char arg_view_native[]   = "240";
    static char arg_linear_flag[]   = "--linear-filter";
    static char arg_uncapped_flag[] = "--uncapped";
    static char arg_hle[]           = "--bios-hle";

    std::vector<char> rom_arg_buf(rom_path.begin(), rom_path.end());
    rom_arg_buf.push_back('\0');

    std::vector<char*> av;
    av.push_back(argv && argv[0] ? argv[0] : arg_prog);
    av.push_back(arg_bios_flag);
    av.push_back(arg_bios_path);
    av.push_back(arg_rom_flag);
    av.push_back(rom_arg_buf.data());
    av.push_back(arg_save_flag);
    av.push_back(arg_save_path);
    av.push_back(arg_view_flag);
    av.push_back(config_widescreen ? arg_view_wide : arg_view_native);
    if (config_bios_hle) {
        av.push_back(arg_hle);
    }
    if (config_linear_filter) {
        av.push_back(arg_linear_flag);
    }
    if (config_uncapped) {
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

    return thread_args.return_code;
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
