// Wario Land 4 (USA/Europe, AWAE) — launcher seam & input validation.
#include "launcher_seam.h"
#include "sha1.h"

int game_launcher_preboot(std::vector<std::string>& args, const gbarecomp::RunOptions& options) {
    return gbarecomp_launcher_preboot(args, options);
}

// Strict verification for retail GBA BIOS and USA/Europe AWAE ROM
int game_validate_inputs(std::vector<std::string>& args, const gbarecomp::RunOptions& options) {
    std::string path, rom_path;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") return 0;
        if (args[i] == "--bios" && i + 1 < args.size()) path = args[++i];
        else if (args[i] == "--rom" && i + 1 < args.size()) rom_path = args[++i];
    }

    const auto exe_dir = std::filesystem::path(gbarecomp_seam::exe_dir(args));

    if (path.empty()) {
        path = gbarecomp_seam::read_single_line((exe_dir / "bios.cfg").string());
    }
    if (path.empty()) {
        if (std::filesystem::exists(exe_dir / "bios" / "gba_bios.bin")) {
            path = (exe_dir / "bios" / "gba_bios.bin").string();
        } else if (std::filesystem::exists(exe_dir / "gba_bios.bin")) {
            path = (exe_dir / "gba_bios.bin").string();
        } else if (std::filesystem::exists("bios/gba_bios.bin")) {
            path = "bios/gba_bios.bin";
        } else if (std::filesystem::exists("gba_bios.bin")) {
            path = "gba_bios.bin";
        }
    }
    if (path.empty()) {
        std::fprintf(stderr, "[wario] Retail GBA BIOS required: select it in the launcher or pass --bios.\n");
        return 1;
    }

    gba::GbaBios bios;
    std::string error;
    if (!bios.load_from_file(path, gba::GbaBios::kExpectedSha1, &error)) {
        std::fprintf(stderr, "[wario] BIOS identity rejected: %s\n", error.c_str());
        return 1;
    }

    if (rom_path.empty()) {
        rom_path = gbarecomp_seam::read_single_line((exe_dir / "rom.cfg").string());
    }
    if (rom_path.empty()) {
        if (std::filesystem::exists(exe_dir / "Wario Land 4 (USA, Europe).gba")) {
            rom_path = (exe_dir / "Wario Land 4 (USA, Europe).gba").string();
        } else if (std::filesystem::exists("Wario Land 4 (USA, Europe).gba")) {
            rom_path = "Wario Land 4 (USA, Europe).gba";
        }
    }

    std::ifstream rom(rom_path, std::ios::binary | std::ios::ate);
    if (!rom || rom.tellg() != 8388608) {
        std::fprintf(stderr, "[wario] Expected the 8 MiB USA/Europe AWAE ROM; select it in the launcher or pass --rom.\n");
        return 1;
    }

    std::vector<std::uint8_t> bytes(8388608);
    rom.seekg(0);
    rom.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!rom || gba::sha1(bytes.data(), bytes.size()).hex() != options.builtin_rom_sha1) {
        std::fprintf(stderr, "[wario] ROM SHA-1 mismatch; expected %s.\n", options.builtin_rom_sha1);
        return 1;
    }

    // Pass validated inputs to runtime
    args.emplace_back("--rom");
    args.emplace_back(rom_path);
    args.emplace_back("--bios");
    args.emplace_back(path);
    args.emplace_back("--rom-sha1");
    args.emplace_back(options.builtin_rom_sha1);
    return 0;
}
