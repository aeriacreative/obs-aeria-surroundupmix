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

# 4. Create ZIP distribution packages in dist
Write-Host "[4/4] Creating distribution ZIP packages..." -ForegroundColor Yellow
if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
}

# 4A. Standard / Portable package (obs-plugins/64bit + data/obs-plugins/aeria-surroundupmix)
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
Write-Host "  -> Created Standard/Portable ZIP: $StandardZip" -ForegroundColor Green

# 4B. ProgramData Standalone package (aeria-surroundupmix/bin/64bit + aeria-surroundupmix/data)
$StagingProgramData = Join-Path $DistDir "staging_programdata"
if (Test-Path $StagingProgramData) { Remove-Item -Path $StagingProgramData -Recurse -Force }

$PdDllDir  = Join-Path $StagingProgramData "aeria-surroundupmix\bin\64bit"
$PdDataDir = Join-Path $StagingProgramData "aeria-surroundupmix\data\locale"
New-Item -ItemType Directory -Path $PdDllDir  -Force | Out-Null
New-Item -ItemType Directory -Path $PdDataDir -Force | Out-Null

Copy-Item $BuiltDll -Destination $PdDllDir -Force
if (Test-Path $BuiltPdb) { Copy-Item $BuiltPdb -Destination $PdDllDir -Force }
Copy-Item "$DataLocaleDir\*" -Destination $PdDataDir -Recurse -Force

$ProgramDataZip = Join-Path $DistDir "aeria-surroundupmix-v$Version-programdata-x64.zip"
if (Test-Path $ProgramDataZip) { Remove-Item $ProgramDataZip -Force }
Compress-Archive -Path "$StagingProgramData\*" -DestinationPath $ProgramDataZip -Force
Remove-Item -Path $StagingProgramData -Recurse -Force
Write-Host "  -> Created ProgramData Standalone ZIP: $ProgramDataZip" -ForegroundColor Green

Write-Host ""
Write-Host "=============================================" -ForegroundColor Green
Write-Host "  Build, Install & Packaging Complete!"       -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green
