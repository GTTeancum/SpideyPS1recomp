[CmdletBinding()]
param([ValidateSet('1','2','both')][string]$Game = 'both', [string]$Output = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$log = $null
$createdOutput = $false
function Run-Dotnet([string[]]$Arguments) {
    Write-Host ('dotnet ' + ($Arguments -join ' '))
    & dotnet @Arguments 2>&1 | Tee-Object -FilePath $script:log -Append | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "dotnet failed with exit $LASTEXITCODE" }
}
try {
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) { throw '.NET 10 SDK is required. No game build was attempted.' }
    Push-Location $root
    try { $sdk = (& dotnet --version).Trim(); $sdkExit = $LASTEXITCODE } finally { Pop-Location }
    if ($sdkExit -ne 0 -or $sdk -notmatch '^10\.') { throw "Expected .NET 10 SDK; found $sdk" }
    if ([string]::IsNullOrWhiteSpace($Output)) { $Output = Join-Path $root ('dist/Linux-06-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
    $Output = [IO.Path]::GetFullPath($Output)
    if ((Test-Path -LiteralPath $Output) -or (Test-Path -LiteralPath ($Output + '.zip'))) { throw 'Output or ZIP already exists. Previous builds are never overwritten.' }
    New-Item -ItemType Directory -Path $Output | Out-Null
    $createdOutput = $true
    $log = Join-Path $Output 'build.log'
    Push-Location $root
    try {
        $core = Join-Path $root 'tools/retarget/native/bin/libOpenSpideyRetarget.so'
        $provenance = Get-Content -Raw -LiteralPath (Join-Path $root 'tools/retarget/native/bin/build-provenance.json') | ConvertFrom-Json
        foreach ($item in $provenance.files) {
            $path = Join-Path $root $item.path
            if ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ne $item.sha256) {
                throw "Native core/source mismatch: $($item.path). Rebuild the Linux core on Linux; do not mix versions."
            }
        }
        $bytes = [IO.File]::ReadAllBytes($core)
        if ($bytes.Length -lt 20 -or $bytes[0] -ne 127 -or $bytes[1] -ne 69 -or $bytes[2] -ne 76 -or $bytes[3] -ne 70 -or $bytes[4] -ne 2 -or [BitConverter]::ToUInt16($bytes,18) -ne 62) { throw 'Linux core is not an x64 ELF.' }
        $sfdManifest = Get-Content -Raw -LiteralPath (Join-Path $root 'tools/movies/native/bin/build-provenance.json') | ConvertFrom-Json
        if ($sfdManifest.abi -ne '0x00010000' -or @($sfdManifest.targets.'linux-x64'.files).Count -lt 5 -or @($sfdManifest.targets.'win-x64'.files).Count -lt 6) { throw 'Missing/incompatible SFD build provenance.' }
        foreach ($item in @($sfdManifest.targets.'linux-x64'.files) + @($sfdManifest.targets.'win-x64'.files)) {
            if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $item.path)).Hash.ToLowerInvariant() -ne $item.sha256) {
                throw "SFD source/native binary mismatch: $($item.path). Rebuild matching SFD component."
            }
        }
        Run-Dotnet @('run','--project','tools/RecompOne/tests/PortabilityRegression','-c','Release','--',
            'spiderman/port/bundled/runtime-assets.zip','spiderman2/port/bundled/runtime-assets.zip')
        Run-Dotnet @('run','--project','tools/RecompOne/tests/MovieOverrideRegression','-c','Release')
        Run-Dotnet @('run','--project','tools/RecompOne/tests/MovieInstructionHookRegression','-c','Release')
        foreach ($spec in @(@('1','spiderman','SpiderMan'), @('2','spiderman2','SpiderMan2'))) {
            if ($Game -ne 'both' -and $Game -ne $spec[0]) { continue }
            $dest = Join-Path $Output $spec[1]
            Run-Dotnet @('publish', "$($spec[1])/port/$($spec[2]).csproj", '-c','Release','-r','linux-x64','--self-contained','true','--nologo','-o',$dest)
            $binary = Join-Path $dest $spec[2]
            $bytes = [IO.File]::ReadAllBytes($binary)
            if ($bytes.Length -lt 20 -or $bytes[0] -ne 127 -or $bytes[1] -ne 69 -or $bytes[2] -ne 76 -or $bytes[3] -ne 70 -or $bytes[4] -ne 2 -or [BitConverter]::ToUInt16($bytes,18) -ne 62) { throw "Publication is not a Linux x64 ELF: $binary" }
            $sfdBytes = [IO.File]::ReadAllBytes((Join-Path $dest 'libOpenSpideySfd.so'))
            if ($sfdBytes.Length -lt 20 -or $sfdBytes[0] -ne 127 -or $sfdBytes[1] -ne 69 -or $sfdBytes[2] -ne 76 -or $sfdBytes[3] -ne 70 -or $sfdBytes[4] -ne 2 -or [BitConverter]::ToUInt16($sfdBytes,18) -ne 62) { throw 'Published SFD component is not a Linux x64 ELF.' }
            if (-not (Test-Path (Join-Path $dest 'licenses/OpenSpideySfd-LICENSE-PL_MPEG.txt'))) { throw 'Missing SFD license in publication.' }
            $launcher = '#!/usr/bin/env bash' + "`n" + 'set -euo pipefail' + "`n" + 'here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"' + "`n" + 'exec "$here/' + $spec[2] + '" "$@"' + "`n"
            [IO.File]::WriteAllText((Join-Path $dest ("Run-" + $spec[2] + '.sh')), $launcher, (New-Object Text.UTF8Encoding($false)))
            if ((Test-Path (Join-Path $dest 'OpenSpideyRetarget.dll')) -or (Test-Path (Join-Path $dest 'OpenSpideySfd.dll')) -or (Get-ChildItem $dest -Filter '*vcruntime*.dll') -or (Get-ChildItem $dest -Filter '*msvcp*.dll')) { throw 'Windows native dependencies leaked into Linux publication.' }
        }
        @"
Native Linux x64 apphosts built from the matching source and preserved-rig core.
On Linux, after extracting, run chmod +x spiderman/SpiderMan spiderman/Run-SpiderMan.sh
(and the corresponding SpiderMan2 paths when built). Then launch Run-SpiderMan.sh.
No disc/data or Dreamcast movies are included. Build success is not a gameplay test.
"@ | Set-Content -LiteralPath (Join-Path $Output 'LINUX-START.txt')
        Compress-Archive -Path (Join-Path $Output '*') -DestinationPath ($Output + '.zip')
        Write-Host "Linux publication complete: $Output.zip"
        Write-Host 'This command cross-builds real ELF apphosts; it does not launch Linux gameplay on Windows.'
    } finally { Pop-Location }
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    if ($createdOutput -and (Test-Path -LiteralPath $Output)) { $_ | Out-String | Set-Content -LiteralPath (Join-Path $Output 'BUILD-FAILED.txt') }
    exit 1
}
