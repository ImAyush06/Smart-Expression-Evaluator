# setup_compiler.ps1
# Automates the setup of WASI SDK 33.0 locally in this folder.

$ErrorActionPreference = "Stop"

$WasiVersion = "33.0"
$Url = "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-33/wasi-sdk-33.0-x86_64-windows.tar.gz"
$TempFile = Join-Path $PSScriptRoot "wasi-sdk.tar.gz"
$DestDir = Join-Path $PSScriptRoot ".wasi-sdk"

# 1. Download wasi-sdk
Write-Host "Downloading WASI SDK v$WasiVersion..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $Url -OutFile $TempFile

# 2. Extract wasi-sdk
Write-Host "Extracting WASI SDK..." -ForegroundColor Cyan
if (Test-Path $DestDir) {
    Write-Host "Removing existing $DestDir directory..."
    Remove-Item -Recurse -Force $DestDir
}
New-Item -ItemType Directory -Path $DestDir | Out-Null

# Run native tar command
tar -xf $TempFile -C $DestDir

# Move contents out of the subfolder wasi-sdk-33.0
$SubDir = Join-Path $DestDir "wasi-sdk-33.0"
if (Test-Path $SubDir) {
    Write-Host "Flattening directory structure..."
    Get-ChildItem -Path $SubDir | ForEach-Object {
        Move-Item -Path $_.FullName -Destination $DestDir -Force
    }
    Remove-Item -Path $SubDir -Force
}

# 3. Clean up
Write-Host "Cleaning up downloaded archive..." -ForegroundColor Cyan
Remove-Item -Path $TempFile -Force

# 4. Verify compiler is runnable
$CompilerPath = Join-Path $DestDir "bin\clang++.exe"
if (Test-Path $CompilerPath) {
    Write-Host "WASI SDK Compiler successfully installed at: $CompilerPath" -ForegroundColor Green
    & $CompilerPath --version
} else {
    Write-Error "Compiler was not found at expected path: $CompilerPath"
}
