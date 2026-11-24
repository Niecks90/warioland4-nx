// Wario Land 4 (USA/Europe, AWAE) — native static recompilation host.
// Windows GUI subsystem entry point: no console window, player-facing recomp-ui launcher.

#include "runtime.h"
#include "runtime_arm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

int game_launcher_preboot(std::vector<std::string>&, const gbarecomp::RunOptions&);
int game_validate_inputs(std::vector<std::string>&, const gbarecomp::RunOptions&);

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

int main(int argc, char** argv) {
    bool developer_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::puts("WarioLand4Recomp [--rom <USA/EU ROM>] [--bios <GBA BIOS>] [game.toml]\n"
                      "Options:\n"
                      "  --launcher         Force GUI launcher\n"
                      "  --window           Run in windowed mode\n"
                      "  --fullscreen       Run in fullscreen mode\n"
                      "  --scale <1..8>     Window scale factor\n"
                      "  --volume <0..100>  Audio volume\n"
                      "  --save <path>      Save file path (default: saves/wario_land_4_awae.sav)\n"
                      "  --no-window        Headless validation mode\n"
                      "  --frames <count>   Stop after specified frame count\n"
                      "  --dev              Developer diagnostics and on-the-fly compiler\n"
                      "\n"
                      "Native 240x160 presentation; cartridge and BIOS hashes are verified.");
            return 0;
        }
        if (std::strcmp(argv[i], "--dev") == 0) {
            developer_mode = true;
        }
    }

    // Normal player mode defaults to compiler-free AOT + interpreter fallback
    // and suppresses diagnostic file output.
    if (!developer_mode) {
#ifdef _WIN32
        if (!std::getenv("GBARECOMP_SELFHEAL_RECOMPILE"))
            _putenv("GBARECOMP_SELFHEAL_RECOMPILE=0");
        if (!std::getenv("GBARECOMP_MISS_FRAG"))
            _putenv("GBARECOMP_MISS_FRAG=NUL");
        if (!std::getenv("GBARECOMP_COVERAGE_JSON"))
            _putenv("GBARECOMP_COVERAGE_JSON=NUL");
        if (!std::getenv("GBARECOMP_SESSION_DIAGNOSTICS"))
            _putenv("GBARECOMP_SESSION_DIAGNOSTICS=0");
#else
        setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 0);
        setenv("GBARECOMP_MISS_FRAG", "/dev/null", 0);
        setenv("GBARECOMP_COVERAGE_JSON", "/dev/null", 0);
        setenv("GBARECOMP_SESSION_DIAGNOSTICS", "0", 0);
#endif
    }

    // Recomp runtime options for Wario Land 4
    gbarecomp::RunOptions options;
    options.builtin_game_name = "Wario Land 4";
    options.builtin_rom_sha1  = "b9fe05a8080e124b67bce6a623234ee3b518a2c1";
    options.builtin_rom_crc32 = 0xD6141609u;
    options.freely_resizable_window = true;
    options.expose_assist_tools = false;
    options.launcher_region = "USA/Europe";
    options.launcher_game_config = "game.toml";
    options.launcher_expose_widescreen = false;
    options.launcher_expose_adaptive_view = false;

    std::vector<std::string> args(argv, argv + argc);
    const bool player_launch = argc == 1 || (argc == 2 && args[1] == "--launcher");

    std::error_code ec;
    auto exe_path = std::filesystem::absolute(argv[0], ec);
    auto exe_dir = ec ? std::filesystem::current_path() : exe_path.parent_path();

    if (player_launch) {
        // The portable launcher, settings, and assets live beside the EXE.
        if (!ec) std::filesystem::current_path(exe_dir, ec);
    }

    // Save path resolution: preserve legacy wario_land_4_awae.sav if present,
    // otherwise use saves/wario_land_4_awae.sav cleanly.
    std::string save_path;
    bool has_cli_save = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if ((args[i] == "--save" || args[i] == "--save-path") && i + 1 < args.size()) {
            has_cli_save = true;
            save_path = args[i + 1];
            break;
        }
    }

    if (!has_cli_save) {
        if (std::filesystem::exists(exe_dir / "saves" / "wario_land_4_awae.sav")) {
            save_path = (exe_dir / "saves" / "wario_land_4_awae.sav").string();
        } else if (std::filesystem::exists(exe_dir / "wario_land_4_awae.sav")) {
            save_path = (exe_dir / "wario_land_4_awae.sav").string();
        } else if (std::filesystem::exists("saves/wario_land_4_awae.sav")) {
            save_path = "saves/wario_land_4_awae.sav";
        } else {
            std::filesystem::create_directories(exe_dir / "saves", ec);
            save_path = (exe_dir / "saves" / "wario_land_4_awae.sav").string();
        }
    }
    options.launcher_save_path = save_path.c_str();

    // Run the recomp-ui GUI launcher preboot
    if (game_launcher_preboot(args, options)) return 0;
    // Strict ROM and BIOS gate
    if (game_validate_inputs(args, options)) return 1;

    if (player_launch) {
        if (!has_cli_save) {
            args.emplace_back("--save");
            args.emplace_back(options.launcher_save_path);
        }
        args.emplace_back("--window");
    }

    // Install dynamic IWRAM stack code interpretation hook
    g_runtime_force_interp_hook = &wario_force_interp_hook;

    std::vector<char*> native_args;
    native_args.reserve(args.size());
    for (auto& arg : args) native_args.push_back(arg.data());

    return gbarecomp::run_game(static_cast<int>(native_args.size()), native_args.data(), options);
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv_w) return 1;

    char** argv = static_cast<char**>(HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(char*) * (argc + 1)));
    if (!argv) {
        LocalFree(argv_w);
        return 1;
    }

    for (int i = 0; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, nullptr, 0, nullptr, nullptr);
        argv[i] = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, n ? n : 1));
        if (argv[i] && n > 0) {
            WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, argv[i], n, nullptr, nullptr);
        }
    }

    int rc = main(argc, argv);

    for (int i = 0; i < argc; ++i) {
        if (argv[i]) HeapFree(GetProcessHeap(), 0, argv[i]);
    }
    HeapFree(GetProcessHeap(), 0, argv);
    LocalFree(argv_w);

    return rc;
}
#endif
