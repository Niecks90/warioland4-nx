<#
.SYNOPSIS
    Package Windows x64 release candidate for WarioLand4Recomp.

.DESCRIPTION
    Creates a clean, standalone release folder and zip archive containing the
    recompiled executable, native UI assets, runtime DLLs, licenses, documentation,
    and a manifest with cryptographic hashes. No private ROM/BIOS/saves included.

.PARAMETER Version
    Release version tag (default: '0.1.0-preview').

.PARAMETER Toolchain
    MinGW-w64 toolchain directory. Discovered automatically if omitted.

.PARAMETER SdlRoot
    SDL2 development prefix. Discovered automatically if omitted.

.PARAMETER SkipBuild
    Skip re-running build-host.ps1 before packaging.
#>
param(
    [string]$Version = '0.1.0-preview',
    [string]$Toolchain = '',
    [string]$SdlRoot = '',
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

if ($Version -notmatch '^\d+\.\d+\.\d+(-[a-z0-9.-]+)?$') {
    throw "Invalid version format '$Version'. Expected semver (e.g. 0.1.0-preview)."
}

# ------------------------------------------------------------- toolchain probe
if (-not $Toolchain) {
    if ($env:MINGW_ROOT -and (Test-Path "$env:MINGW_ROOT/bin/gcc.exe")) {
        $Toolchain = $env:MINGW_ROOT
    } else {
        $gccCmd = Get-Command gcc.exe -ErrorAction SilentlyContinue
        if ($gccCmd) {
            $Toolchain = Split-Path -Parent (Split-Path -Parent $gccCmd.Source)
        }
    }
}

if (-not $SdlRoot) {
    if ($env:SDL2_ROOT -and (Test-Path "$env:SDL2_ROOT/include/SDL2/SDL.h")) {
        $SdlRoot = $env:SDL2_ROOT
    } else {
        $sibling = Join-Path (Split-Path -Parent $root) '_sdl2\SDL2-2.32.8\x86_64-w64-mingw32'
        if (Test-Path "$sibling/include/SDL2/SDL.h") {
            $SdlRoot = $sibling
        }
    }
}

# ------------------------------------------------------------- dependency check
$pin = Get-Content "$root/docs/FRAMEWORK_PIN.json" -Raw | ConvertFrom-Json
$checks = @(
    @('reference/gbarecomp', $pin.framework.upstream_commit),
    @('recomp-ui', $pin.dependencies.'recomp-ui'.commit)
)

foreach ($pair in $checks) {
    $dir = Join-Path $root $pair[0]
    if (Test-Path -LiteralPath $dir) {
        $head = (& git -C $dir rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or $head -ne $pair[1]) {
            throw "Dependency pin mismatch for $($pair[0]): expected $($pair[1]), got $head"
        }
    }
}

# ------------------------------------------------------------------ host build
if (-not $SkipBuild) {
    Write-Host "==> Building host executable (Release, -Jobs 2)..." -ForegroundColor Cyan
    $buildArgs = @{ Jobs = 2 }
    if ($Toolchain) { $buildArgs['Toolchain'] = $Toolchain }
    if ($SdlRoot) { $buildArgs['SdlRoot'] = $SdlRoot }
    & "$root/tools/regeneration/build-host.ps1" @buildArgs
}

if (-not (Select-String -LiteralPath "$root/build/host/CMakeCache.txt" -Pattern '^CMAKE_BUILD_TYPE:STRING=Release$')) {
    throw 'Release build required. Re-run without -SkipBuild or build with CMAKE_BUILD_TYPE=Release.'
}

# -------------------------------------------------------------- staging layout
$name = "WarioLand4Recomp-windows-x64-v$Version"
$stageDir = Join-Path $root 'release-stage'
$stage = Join-Path $stageDir $name
$distDir = Join-Path $root 'dist'
$zip = Join-Path $distDir "$name.zip"

if (Test-Path -LiteralPath $stage) { Remove-Item -Recurse -Force -LiteralPath $stage }
if (-not (Test-Path -LiteralPath $distDir)) { New-Item -ItemType Directory -Path $distDir -Force | Out-Null }
if (Test-Path -LiteralPath $zip) { Remove-Item -Force -LiteralPath $zip }
if (Test-Path -LiteralPath "$zip.sha256") { Remove-Item -Force -LiteralPath "$zip.sha256" }

New-Item -ItemType Directory -Path "$stage/licenses/mpl-source" -Force | Out-Null
New-Item -ItemType Directory -Path "$stage/assets" -Force | Out-Null

# ------------------------------------------------------------ copy executables
$binaries = @('WarioLand4Recomp.exe', 'SDL2.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')
foreach ($b in $binaries) {
    $src = Join-Path "$root/build/host" $b
    if (-not (Test-Path -LiteralPath $src)) { throw "Required binary missing: $src" }
    Copy-Item -LiteralPath $src -Destination "$stage/$b"
    
    # Strip debug symbols if strip.exe is available
    $stripExe = if ($Toolchain) { Join-Path $Toolchain 'bin/strip.exe' } else { (Get-Command strip.exe -ErrorAction SilentlyContinue)?.Source }
    if ($stripExe -and (Test-Path $stripExe)) {
        & $stripExe --strip-debug "$stage/$b"
    }
}

# ----------------------------------------------------------------- copy assets
$assetDirs = @('fonts', 'img')
foreach ($ad in $assetDirs) {
    $srcAd = Join-Path "$root/build/host/assets" $ad
    if (Test-Path -LiteralPath $srcAd) {
        Copy-Item -Recurse -LiteralPath $srcAd -Destination "$stage/assets/$ad"
    }
}

# ------------------------------------------------------------- copy root files
Copy-Item -LiteralPath "$root/game.toml" -Destination "$stage/game.toml"
Copy-Item -LiteralPath "$root/LICENSE" -Destination "$stage/LICENSE"
Copy-Item -LiteralPath "$root/THIRD_PARTY_NOTICES.md" -Destination "$stage/THIRD_PARTY_NOTICES.md"
Copy-Item -LiteralPath "$root/README.md" -Destination "$stage/README.md"
if (Test-Path -LiteralPath "$root/README.zh-CN.md") {
    Copy-Item -LiteralPath "$root/README.zh-CN.md" -Destination "$stage/README.zh-CN.md"
}

# --------------------------------------------------------------- copy licenses
$licenses = [ordered]@{
    'GBARecomp.txt'             = "$root/reference/gbarecomp/LICENSE"
    'RecompUI.txt'              = "$root/recomp-ui/LICENSE"
    'ImGui.txt'                 = "$root/recomp-ui/src/third_party/imgui/LICENSE.txt"
    'ArmRecompCore.txt'         = "$root/reference/gbarecomp/external/arm-recomp-core/LICENSE"
    'RecompNet.txt'             = "$root/reference/gbarecomp/external/recomp-net/LICENSE"
    'Rbengine.txt'              = "$root/reference/gbarecomp/external/rbengine/LICENSE"
    'Font-Notices.txt'          = "$root/recomp-ui/assets/common/fonts/NOTICE.md"
    'Image-Notices.txt'         = "$root/recomp-ui/assets/common/img/NOTICE.md"
    'MPL-2.0.txt'               = "$root/reference/gbarecomp/third_party/MPL-2.0.txt"
    'Framework-Attribution.txt' = "$root/reference/gbarecomp/THIRD_PARTY_ATTRIBUTION.md"
}

if ($SdlRoot) {
    $sdlLic = Join-Path (Split-Path -Parent $SdlRoot) 'LICENSE.txt'
    if (Test-Path $sdlLic) { $licenses['SDL2.txt'] = $sdlLic }
}

foreach ($item in $licenses.GetEnumerator()) {
    if (Test-Path -LiteralPath $item.Value) {
        Copy-Item -LiteralPath $item.Value -Destination "$stage/licenses/$($item.Key)"
    }
}

foreach ($f in @('bios_hle.cpp', 'bios_hle.h')) {
    $src = "$root/reference/gbarecomp/src/runtime/$f"
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination "$stage/licenses/mpl-source/$f"
    }
}

# ------------------------------------------------- verify no prohibited files
$prohibited = @('*.gba', '*.agb', '*.bin', '*.sav', '*.srm', '*.o', '*.obj', '*.pdb', '*.ilk', '*.log')
$foundProhibited = @(Get-ChildItem -LiteralPath $stage -Recurse -Include $prohibited)
if ($foundProhibited.Count -gt 0) {
    throw "Prohibited files detected in release stage: $($foundProhibited.FullName -join ', ')"
}

# ----------------------------------------------------------- manifest creation
$files = Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName
$manifest = @(foreach ($f in $files) {
    $rel = $f.FullName.Substring($stage.Length + 1).Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    [pscustomobject]@{
        file   = $rel
        bytes  = $f.Length
        sha256 = $hash
    }
})

$manifestJson = Join-Path $stage 'manifest.json'
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestJson -Encoding utf8

# ------------------------------------------------------------- compress archive
Write-Host "==> Creating release archive: $zip" -ForegroundColor Cyan
Compress-Archive -Path "$stage/*" -DestinationPath $zip -CompressionLevel Optimal

$zipItem = Get-Item -LiteralPath $zip
$zipHash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$zipHash  $($zipItem.Name)" | Set-Content -LiteralPath "$zip.sha256" -Encoding ascii

Write-Host ""
Write-Host "=====================================================================" -ForegroundColor Green
Write-Host "PASS: Windows x64 Release Candidate created successfully!" -ForegroundColor Green
Write-Host "      Stage:   $stage" -ForegroundColor Green
Write-Host "      Archive: $zip" -ForegroundColor Green
Write-Host "      Size:    $($zipItem.Length) bytes ($([math]::Round($zipItem.Length / 1MB, 2)) MB)" -ForegroundColor Green
Write-Host "      SHA256:  $zipHash" -ForegroundColor Green
Write-Host "=====================================================================" -ForegroundColor Green
