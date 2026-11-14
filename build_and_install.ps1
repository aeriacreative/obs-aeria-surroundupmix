# Build and Install Script for aeria-surroundupmix
# Aeria Creative — linktr.ee/aeriacreative

$ErrorActionPreference = "Stop"

$PluginDir = $PSScriptRoot
$WorkspaceRoot = "D:\PROJECT\AERIA CREATIVE\OBS PLUGIN BY AERIA CREATIVE"
$ObsPortableDir = Join-Path $WorkspaceRoot "OBS-Studio-33.0.1-Windows-x64"
if (-not (Test-Path $ObsPortableDir)) {
    $DetectedObs = Get-ChildItem -Path $WorkspaceRoot -Directory -Filter "OBS-Studio-*-Windows-x64" -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if ($DetectedObs) { $ObsPortableDir = $DetectedObs.FullName }
}
$DistDir = Join-Path $PluginDir "dist"
$Version = "0.1.4"

# Locate CMake
$CMakeExe = "cmake"
if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    if (Test-Path "C:\Program Files\CMake\bin\cmake.exe") {
        $CMakeExe = "C:\Program Files\CMake\bin\cmake.exe"
    }
}

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "  Building aeria-surroundupmix plugin v$Version..." -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

Set-Location $PluginDir

# 1. CMake Configure
Write-Host "[1/4] Configuring CMake..." -ForegroundColor Yellow
& $CMakeExe --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 2. CMake Build
Write-Host "[2/4] Compiling Plugin (RelWithDebInfo)..." -ForegroundColor Yellow
& $CMakeExe --build --preset windows-x64 --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 3. Copy to Portable OBS
Write-Host "[3/4] Installing to Portable OBS Studio..." -ForegroundColor Yellow

$BuiltDll = Join-Path $PluginDir "build_x64\RelWithDebInfo\aeria-surroundupmix.dll"
$BuiltPdb = Join-Path $PluginDir "build_x64\RelWithDebInfo\aeria-surroundupmix.pdb"
$DataLocaleDir = Join-Path $PluginDir "data\locale"

if (-not (Test-Path $BuiltDll)) {
    Write-Error "DLL not found at $BuiltDll"
}

# In OBS 33+, third-party plugins in portable mode use: <obs-root>\plugins\<plugin-name>\
# Legacy structure (<obs-root>\obs-plugins\64bit\) is flagged as Legacy.
$IsObs33OrNewer = (Test-Path (Join-Path $ObsPortableDir "core"))

if ($IsObs33OrNewer) {
    $TargetPluginDir = Join-Path $ObsPortableDir "plugins\aeria-surroundupmix"
    $TargetLocaleDir = Join-Path $TargetPluginDir "data\locale"
    if (-not (Test-Path $TargetLocaleDir)) { New-Item -ItemType Directory -Path $TargetLocaleDir -Force | Out-Null }

    Copy-Item $BuiltDll -Destination $TargetPluginDir -Force
    if (Test-Path $BuiltPdb) { Copy-Item $BuiltPdb -Destination $TargetPluginDir -Force }
    Copy-Item "$DataLocaleDir\*" -Destination $TargetLocaleDir -Recurse -Force
    Write-Host "  -> Installed Modern Plugin: $TargetPluginDir" -ForegroundColor Green

    # Clean legacy paths so OBS does not show Legacy warning
    Remove-Item -Path "$ObsPortableDir\obs-plugins\64bit\aeria-surroundupmix.*" -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "$ObsPortableDir\data\obs-plugins\aeria-surroundupmix" -Recurse -Force -ErrorAction SilentlyContinue
} else {
    $TargetDllDir  = Join-Path $ObsPortableDir "obs-plugins\64bit"
    $TargetDataDir = Join-Path $ObsPortableDir "data\obs-plugins\aeria-surroundupmix\locale"
    if (-not (Test-Path $TargetDllDir))  { New-Item -ItemType Directory -Path $TargetDllDir  -Force | Out-Null }
    if (-not (Test-Path $TargetDataDir)) { New-Item -ItemType Directory -Path $TargetDataDir -Force | Out-Null }

    Copy-Item $BuiltDll -Destination $TargetDllDir -Force
    if (Test-Path $BuiltPdb) { Copy-Item $BuiltPdb -Destination $TargetDllDir -Force }
    Copy-Item "$DataLocaleDir\*" -Destination $TargetDataDir -Recurse -Force
    Write-Host "  -> Installed Legacy Plugin: $TargetDllDir\aeria-surroundupmix.dll" -ForegroundColor Green
}

# 4. Create ZIP distribution packages in dist
Write-Host "[4/4] Creating distribution ZIP packages..." -ForegroundColor Yellow
if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
}

# 4A. Standard / Legacy package (obs-plugins/64bit + data/obs-plugins/aeria-surroundupmix)
# For OBS <= 32 or manual Program Files installation
$StagingStandard = Join-Path $DistDir "staging_standard"
if (Test-Path $StagingStandard) { Remove-Item -Path $StagingStandard -Recurse -Force }

$StdDllDir  = Join-Path $StagingStandard "obs-plugins\64bit"
$StdDataDir = Join-Path $StagingStandard "data\obs-plugins\aeria-surroundupmix\locale"
New-Item -ItemType Directory -Path $StdDllDir  -Force | Out-Null
New-Item -ItemType Directory -Path $StdDataDir -Force | Out-Null

Copy-Item $BuiltDll -Destination $StdDllDir -Force
if (Test-Path $BuiltPdb) { Copy-Item $BuiltPdb -Destination $StdDllDir -Force }
Copy-Item "$DataLocaleDir\*" -Destination $StdDataDir -Recurse -Force

$StandardZip = Join-Path $DistDir "aeria-surroundupmix-v$Version-windows-x64.zip"
if (Test-Path $StandardZip) { Remove-Item $StandardZip -Force }
Compress-Archive -Path "$StagingStandard\*" -DestinationPath $StandardZip -Force
Remove-Item -Path $StagingStandard -Recurse -Force
Write-Host "  -> Created Standard/Legacy ZIP: $StandardZip" -ForegroundColor Green

# 4B. Modern ProgramData / Portable package (OBS 33+ New Format: no bin/64bit subfolder!)
# Layout: aeria-surroundupmix\aeria-surroundupmix.dll and aeria-surroundupmix\data\locale\*
$StagingProgramData = Join-Path $DistDir "staging_programdata"
if (Test-Path $StagingProgramData) { Remove-Item -Path $StagingProgramData -Recurse -Force }

$PdBaseDir = Join-Path $StagingProgramData "aeria-surroundupmix"
$PdDataDir = Join-Path $PdBaseDir "data\locale"
New-Item -ItemType Directory -Path $PdDataDir -Force | Out-Null

Copy-Item $BuiltDll -Destination $PdBaseDir -Force
if (Test-Path $BuiltPdb) { Copy-Item $BuiltPdb -Destination $PdBaseDir -Force }
Copy-Item "$DataLocaleDir\*" -Destination $PdDataDir -Recurse -Force

$ProgramDataZip = Join-Path $DistDir "aeria-surroundupmix-v$Version-programdata-x64.zip"
if (Test-Path $ProgramDataZip) { Remove-Item $ProgramDataZip -Force }
Compress-Archive -Path "$StagingProgramData\*" -DestinationPath $ProgramDataZip -Force
Remove-Item -Path $StagingProgramData -Recurse -Force
Write-Host "  -> Created Modern ProgramData ZIP (OBS 33+ Format): $ProgramDataZip" -ForegroundColor Green

Write-Host ""
Write-Host "=============================================" -ForegroundColor Green
Write-Host "  Build, Install & Packaging Complete!"       -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green
