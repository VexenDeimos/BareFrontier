[CmdletBinding()]
param(
    [ValidateSet('releasedbg','debug')][string]$Mode = 'releasedbg',
    [string]$GameRoot,
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) { throw 'Install xmake before building.' }
& (Join-Path $PSScriptRoot 'Setup-Dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency setup failed.' }
Push-Location $root
try {
    & xmake f -m $Mode
    if ($LASTEXITCODE -ne 0) { throw 'xmake configuration failed.' }
    & xmake build BareFrontier
    if ($LASTEXITCODE -ne 0) { throw 'xmake build failed.' }
} finally { Pop-Location }
$built = Join-Path $root "build\windows\x64\$Mode\BareFrontier.dll"
if (-not (Test-Path -LiteralPath $built)) { throw "Expected DLL not found: $built" }
Write-Host "Built DLL: $built" -ForegroundColor Green
if ($Install) {
    if (-not $GameRoot) { throw 'Specify -GameRoot when using -Install.' }
    if (Get-Process -Name Starfield -ErrorAction SilentlyContinue) { throw 'Close Starfield before installing.' }
    $targetDir = Join-Path $GameRoot 'Data\SFSE\Plugins'
    if (-not (Test-Path -LiteralPath $targetDir)) { throw "SFSE plugin folder missing: $targetDir" }
    $target = Join-Path $targetDir 'BareFrontier.dll'
    if (Test-Path -LiteralPath $target) {
        $backup = "$target.bak-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
        Copy-Item -LiteralPath $target -Destination $backup -Force
        Write-Host "Backed up previous DLL: $backup" -ForegroundColor Yellow
    }
    Copy-Item -LiteralPath $built -Destination $target -Force
    if ((Get-FileHash $built -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) {
        throw 'DLL copy verification failed.'
    }
    Write-Host "Installed DLL: $target" -ForegroundColor Green
}
