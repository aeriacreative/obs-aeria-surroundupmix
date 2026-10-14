# Build and Install Script for aeria-surroundupmix
# Aeria Creative — linktr.ee/aeriacreative

$ErrorActionPreference = "Stop"

$PluginDir = $PSScriptRoot
$ObsPortableDir = "d:\PROJECT\AERIA CREATIVE\OBS PLUGIN BY AERIA CREATIVE\OBS-Studio-32.2.2-Windows-x64"
$DistDir = Join-Path $PluginDir "dist"
$Version = "0.0.5"

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

$TargetDllDir  = Join-Path $ObsPortableDir "obs-plugins\64bit"
$TargetDataDir = Join-Path $ObsPortableDir "data\obs-plugins\aeria-surroundupmix"

if (-not (Test-Path $TargetDllDir))  { New-Item -ItemType Directory -Path $TargetDllDir  -Force | Out-Null }
if (-not (Test-Path $TargetDataDir)) { New-Item -ItemType Directory -Path $TargetDataDir -Force | Out-Null }

$BuiltDll = Join-Path $PluginDir "build_x64\RelWithDebInfo\aeria-surroundupmix.dll"
$BuiltPdb = Join-Path $PluginDir "build_x64\RelWithDebInfo\aeria-surroundupmix.pdb"

if (Test-Path $BuiltDll) {
    Copy-Item $BuiltDll -Destination $TargetDllDir -Force
    Write-Host "  -> Installed DLL: $TargetDllDir\aeria-surroundupmix.dll" -ForegroundColor Green
} else {
    Write-Error "DLL not found at $BuiltDll"
}

if (Test-Path $BuiltPdb) {
    Copy-Item $BuiltPdb -Destination $TargetDllDir -Force
    Write-Host "  -> Installed PDB: $TargetDllDir\aeria-surroundupmix.pdb" -ForegroundColor Green
}

$DataLocaleDir = Join-Path $PluginDir "data\locale"
if (Test-Path $DataLocaleDir) {
    $TargetLocaleDir = Join-Path $TargetDataDir "locale"
    if (-not (Test-Path $TargetLocaleDir)) { New-Item -ItemType Directory -Path $TargetLocaleDir -Force | Out-Null }
    Copy-Item "$DataLocaleDir\*" -Destination $TargetLocaleDir -Recurse -Force
    Write-Host "  -> Installed Data/Locale to $TargetLocaleDir" -ForegroundColor Green
}

# 4. Create ZIP package in dist
Write-Host "[4/4] Creating distribution ZIP package..." -ForegroundColor Yellow
if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
}

$ZipStagingDir = Join-Path $DistDir "staging"
if (Test-Path $ZipStagingDir) {
    Remove-Item -Path $ZipStagingDir -Recurse -Force
}

$StagingDllDir  = Join-Path $ZipStagingDir "obs-plugins\64bit"
$StagingDataDir = Join-Path $ZipStagingDir "data\obs-plugins\aeria-surroundupmix\locale"
New-Item -ItemType Directory -Path $StagingDllDir -Force | Out-Null
New-Item -ItemType Directory -Path $StagingDataDir -Force | Out-Null

Copy-Item $BuiltDll -Destination $StagingDllDir -Force
if (Test-Path $BuiltPdb) { Copy-Item $BuiltPdb -Destination $StagingDllDir -Force }
Copy-Item "$DataLocaleDir\*" -Destination $StagingDataDir -Recurse -Force

$ZipOutput = Join-Path $DistDir "aeria-surroundupmix-v$Version-windows-x64.zip"
if (Test-Path $ZipOutput) {
    Remove-Item $ZipOutput -Force
}

Compress-Archive -Path "$ZipStagingDir\*" -DestinationPath $ZipOutput -Force
Remove-Item -Path $ZipStagingDir -Recurse -Force

Write-Host "  -> Created ZIP package: $ZipOutput" -ForegroundColor Green

Write-Host ""
Write-Host "=============================================" -ForegroundColor Green
Write-Host "  Build, Install & Packaging Complete!"       -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green
