[CmdletBinding()]
param([string]$Game = 'both', [string]$Output = '')
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Game)) { $Game = 'both' }
if ($Game -notin @('1','2','both')) { Write-Error 'Game must be 1, 2 or both'; exit 2 }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$created = $false
function Run-Dotnet([string[]]$Arguments) {
    Write-Host ('dotnet ' + ($Arguments -join ' '))
    & dotnet @Arguments 2>&1 | Tee-Object -FilePath $script:log -Append | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "dotnet failed with exit $LASTEXITCODE" }
}
try {
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) { throw '.NET 10 SDK is required. No game build was attempted.' }
    Push-Location $root
    try {
        $sdk = (& dotnet --version).Trim()
        if ($LASTEXITCODE -ne 0 -or $sdk -notmatch '^10\.') { throw "Expected .NET 10 SDK; found $sdk" }
        if ([string]::IsNullOrWhiteSpace($Output)) { $Output = Join-Path $root ('dist/Windows-06-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
        $Output = [IO.Path]::GetFullPath($Output)
        if (Test-Path -LiteralPath $Output) { throw 'Output already exists. Previous builds are never overwritten.' }
        New-Item -ItemType Directory -Path $Output | Out-Null
        $created = $true; $log = Join-Path $Output 'build.log'
        $sfdManifest = Get-Content -Raw -LiteralPath (Join-Path $root 'tools/movies/native/bin/build-provenance.json') | ConvertFrom-Json
        if ($sfdManifest.abi -ne '0x00010000' -or @($sfdManifest.targets.'win-x64'.files).Count -lt 6) { throw 'Missing/incompatible SFD build provenance.' }
        foreach ($item in $sfdManifest.targets.'win-x64'.files) {
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
            Run-Dotnet @('publish', "$($spec[1])/port/$($spec[2]).csproj", '-c','Release','-r','win-x64','--self-contained','true','--nologo','-o',$dest)
            if (-not (Test-Path (Join-Path $dest ($spec[2] + '.exe')))) { throw 'Expected game EXE missing' }
            if (-not (Test-Path (Join-Path $dest 'licenses/OpenSpideySfd-LICENSE-PL_MPEG.txt'))) { throw 'Missing SFD license in publication.' }
        }
        'Built from matching Repair06 source. Publication is not a gameplay test. Original SFDs belong under mods/movies/dreamcast beside the NEW SM1 EXE; disc extraction installer is deferred.' |
            Set-Content -LiteralPath (Join-Path $Output 'BUILD-SUCCEEDED.txt')
        Write-Host "Matching Windows output: $Output"
    } finally { Pop-Location }
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    if ($created) { $_ | Out-String | Set-Content -LiteralPath (Join-Path $Output 'BUILD-FAILED.txt') }
    exit 1
}
