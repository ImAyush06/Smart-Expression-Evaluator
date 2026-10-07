# build.ps1
# Compiles C++ code into calculator.wasm using the locally setup WASI SDK.

$ErrorActionPreference = "Stop"

$ClangPath = Join-Path $PSScriptRoot ".wasi-sdk\bin\clang++.exe"
$SysrootPath = Join-Path $PSScriptRoot ".wasi-sdk\share\wasi-sysroot"
$OutFile = Join-Path $PSScriptRoot "calculator.wasm"

if (-not (Test-Path $ClangPath)) {
    Write-Error "WASI SDK Clang++ compiler not found at: $ClangPath. Please run setup_compiler.ps1 first."
}

Write-Host "Compiling C++ evaluator to WebAssembly..." -ForegroundColor Cyan

& $ClangPath `
    "-O3" `
    "-std=c++17" `
    "-fno-exceptions" `
    "--target=wasm32-wasi" `
    "--sysroot=$SysrootPath" `
    "-nostartfiles" `
    "-Wl,--no-entry" `
    "-Wl,--export-all" `
    "-o" $OutFile `
    (Join-Path $PSScriptRoot "evaluator.cpp") `
    (Join-Path $PSScriptRoot "wasm_bindings.cpp")

if (Test-Path $OutFile) {
    $FileSize = (Get-Item $OutFile).Length
    Write-Host "WebAssembly module successfully built: $OutFile ($($FileSize / 1024) KB)" -ForegroundColor Green
} else {
    Write-Error "WebAssembly compilation failed, calculator.wasm was not generated."
}
