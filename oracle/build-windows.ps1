param(
    [ValidateSet('capture','steal')][string]$Kind='capture',
    [string]$Compiler='C:\msys64\mingw32\bin\gcc.exe',
    [string]$OutputDirectory=''
)
$ErrorActionPreference='Stop'
$oracleRoot=$PSScriptRoot
if(-not $OutputDirectory){$OutputDirectory=Join-Path $oracleRoot ('build-windows-'+$Kind)}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$source=if($Kind -eq 'capture'){'aweman_proxy.c'}else{'steal_fixture.c'}
$compilerDirectory=Split-Path -Parent $Compiler
$env:PATH=$compilerDirectory+';'+$env:PATH
[string[]]$captureFlags=if($Kind -eq 'capture'){@('-DRR_EMBEDDED_CAPTURE=1')}else{@()}
& $Compiler @captureFlags -O2 -Wall -shared -static-libgcc -o (Join-Path $OutputDirectory 'AWEMAN32.DLL') (Join-Path $oracleRoot $source) (Join-Path $oracleRoot 'aweman_proxy.def') -lwinmm -lgdi32 '-Wl,--enable-stdcall-fixup'
if($LASTEXITCODE -ne 0){throw '32-bit oracle DLL build failed'}
