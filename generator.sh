#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

# 1. Build host gba_recompile if needed
if [ ! -x build_host/gba_recompile ]; then
    echo "==> Building host build_host/gba_recompile..."
    cmake -S gbarecomp -B build_host \
        -DCMAKE_BUILD_TYPE=Release \
        -DGBARECOMP_TOMLPP_INCLUDE_DIR="$ROOT_DIR/upstream_recomp/reference/tomlplusplus"
    cmake --build build_host --target gba_recompile -j"$(nproc)"
fi

# 2. Locate Wario Land 4 (USA, Europe) ROM
ROM_PATH="${1:-}"
if [ -z "$ROM_PATH" ]; then
    for cand in \
        "$ROOT_DIR/Wario Land 4 (USA, Europe).gba" \
        "$ROOT_DIR/rom.gba" \
        "$ROOT_DIR/upstream_recomp/Wario Land 4 (USA, Europe).gba"
    do
        if [ -f "$cand" ]; then
            ROM_PATH="$cand"
            break
        fi
    done
fi

if [ -z "$ROM_PATH" ] || [ ! -f "$ROM_PATH" ]; then
    echo "ERROR: Wario Land 4 (USA, Europe) ROM not found." >&2
    echo "Usage: ./generator.sh [path/to/Wario Land 4 (USA, Europe).gba]" >&2
    exit 1
fi

# 3. Clean generated/ so stale shards/maps cannot survive across regenerations
mkdir -p generated
find generated -mindepth 1 -maxdepth 1 -exec rm -rf {} +

# 4. Generate sharded C++ corpus + kDispatchTable into generated/
./build_host/gba_recompile \
    --rom "$ROM_PATH" \
    --config game.toml \
    --config symbols/AWAE_symbols.toml \
    --symbols symbols/imported_symbols.tsv \
    --data-symbols symbols/imported_data_symbols.tsv \
    --out generated \
    --max-functions 65536
