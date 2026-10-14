[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dependency = Join-Path $root 'lib\commonlibsf'

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git is required.' }
if (Test-Path -LiteralPath $dependency) {
    if (-not (Test-Path -LiteralPath (Join-Path $dependency 'xmake.lua'))) {
        throw "Existing CommonLibSF folder appears incomplete: $dependency"
    }
    Write-Host "CommonLibSF already present at $dependency" -ForegroundColor Green
    exit 0
}
New-Item -Path (Join-Path $root 'lib') -ItemType Directory -Force | Out-Null
& git clone --recurse-submodules 'https://github.com/libxse/commonlibsf.git' $dependency
if ($LASTEXITCODE -ne 0) { throw 'Failed cloning CommonLibSF.' }
Write-Host 'CommonLibSF and its submodules cloned successfully.' -ForegroundColor Green
