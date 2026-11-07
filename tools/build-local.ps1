param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [switch]$ConfigureOnly, [string]$Target='')
$ErrorActionPreference='Stop'
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install Visual Studio 2022 C++ desktop tools.' }
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$lines=& cmd.exe /d /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE) { throw "MSVC environment initialization failed" }
foreach ($line in $lines) { if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') } }
$repo=Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
  $preset='windows-local-'+$Configuration.ToLowerInvariant()
  & cmake --preset $preset
  if ($LASTEXITCODE) { exit $LASTEXITCODE }
  if (!$ConfigureOnly) {
    if ($Target) { & cmake --build --preset $preset --parallel 6 --target $Target }
    else { & cmake --build --preset $preset --parallel 6 }
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
  }
} finally { Pop-Location }
