[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RepositoryUrl
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git is required.' }
if ($RepositoryUrl -notmatch '^https://github\.com/VexenDeimos/[A-Za-z0-9._-]+(?:\.git)?/?$') {
    throw 'Provide the new repository HTTPS URL under VexenDeimos (for example https://github.com/VexenDeimos/BareFrontier.git).'
}
if (Test-Path -LiteralPath (Join-Path $root '.git')) {
    throw 'This folder already has Git history. Publish manually to avoid altering its remotes or history.'
}
Push-Location $root
try {
    & git init -b main
    if ($LASTEXITCODE -ne 0) { throw 'git init failed.' }
    & git add -- src/main.cpp src/pch.h xmake.lua README.md .gitignore .gitattributes scripts/Setup-Dependencies.ps1 scripts/Build.ps1 scripts/Publish-To-GitHub.ps1
    if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }
    & git commit -m 'Initial Bare Frontier v0.1.5j source release'
    if ($LASTEXITCODE -ne 0) { throw 'git commit failed; configure Git user.name and user.email.' }
    & git remote add origin $RepositoryUrl
    if ($LASTEXITCODE -ne 0) { throw 'Could not configure origin remote.' }
    & git push -u origin main
    if ($LASTEXITCODE -ne 0) { throw 'git push failed; verify the empty repository exists and you are logged into GitHub.' }
    Write-Host "Published Bare Frontier to $RepositoryUrl" -ForegroundColor Green
} finally { Pop-Location }
