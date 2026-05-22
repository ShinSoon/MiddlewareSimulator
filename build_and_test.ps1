<#
.SYNOPSIS
    Build the MiddlewareSimulator solution and run the GoogleTest suite.

.DESCRIPTION
    Locates MSBuild via vswhere, builds the solution for the given configuration
    and platform, then runs the test executable. Exits with the test runner's
    exit code (0 = all tests passed).

.EXAMPLE
    .\build_and_test.ps1
    .\build_and_test.ps1 -Configuration Release
#>
param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot
$solution = Join-Path $repoRoot "MiddlewareSimulator\MiddlewareSimulator.sln"

if (-not (Test-Path $solution)) { throw "Solution not found: $solution" }

# Locate MSBuild via vswhere (ships with the Visual Studio Installer).
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found. Is Visual Studio installed?" }

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe not found via vswhere." }

Write-Host "Building $solution  ($Configuration | $Platform)..." -ForegroundColor Cyan
& $msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m /nologo /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }

$testExe = Join-Path $repoRoot "MiddlewareSimulator\$Platform\$Configuration\MiddlewareSimulatorTests.exe"
if (-not (Test-Path $testExe)) { throw "Test executable not found: $testExe" }

Write-Host "Running tests: $testExe" -ForegroundColor Cyan
& $testExe
exit $LASTEXITCODE
