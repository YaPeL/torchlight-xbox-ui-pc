# Checks a release's Windows installer where there is no Python or compiler (the release
# workflow's Windows Server Core container), on tests/release_fixture.py's synthetic games, as
# check_release.sh does on Linux: each install puts the release's proxy in place, and each
# uninstall leaves the folder as it was (hashes).
# Usage: check_release.ps1 INSTALLER FIXTURE_DIR GOG_PROXY STEAM_PROXY
param([string]$Installer, [string]$Fixture, [string]$GogProxy, [string]$SteamProxy)
$ErrorActionPreference = "Stop"
function Fail($text) { Write-Error "check_release: $text"; exit 1 }
function Tree($dir) {
  Get-ChildItem -LiteralPath $dir -Recurse -File | Sort-Object FullName |
    ForEach-Object { $_.FullName.Substring($dir.Length) + " " + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
}
function Hash($file) { (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash }
if (Get-Command python -ErrorAction SilentlyContinue) { Fail "Python is on this machine" }
if (Get-Command clang -ErrorAction SilentlyContinue) { Fail "clang is on this machine" }
$env:USERPROFILE = Join-Path $Fixture "home"
$env:APPDATA = Join-Path $Fixture "home\AppData\Roaming"
New-Item -ItemType Directory -Force -Path $env:USERPROFILE | Out-Null
$saves = Join-Path $Fixture "prefix\drive_c\users\me\AppData\Roaming\runic games\torchlight\save"

function One($store, $game, $proxy, $bag, $extra) {
  $before = (Tree $game) -join "`n"
  & $Installer install --game $game --x360-dir (Join-Path $Fixture "x360") --yes --console --bag $bag @extra
  if ($LASTEXITCODE -ne 0) { Fail "${store}: install" }
  if ((Hash (Join-Path $game "CEGUIBase.dll")) -ne (Hash $proxy)) { Fail "${store}: the installed CEGUIBase.dll is not the release's proxy" }
  if (-not (Select-String -LiteralPath (Join-Path $game "x360_ui.ini") -Pattern "^bag=$bag" -Quiet)) { Fail "${store}: no bag=$bag in the ini" }
  foreach ($d in "x360ui", "x360ui-playstation3", "x360ui-playstation4", "x360ui-playstation5", "x360ui-nintendo", "x360ui-nintendo-swapped") {
    if (-not (Test-Path -LiteralPath (Join-Path $game "$d\x360_ui_report.json"))) { Fail "${store}: no $d" }
  }
  & $Installer uninstall --game $game --yes --console --anyway
  if ($LASTEXITCODE -ne 0) { Fail "${store}: uninstall" }
  if (((Tree $game) -join "`n") -ne $before) { Fail "${store}: the folder is not as it was after uninstalling" }
  Write-Output "check_release: $store installed and uninstalled"
}

$out = & $Installer list 2>&1 | Out-String
if ($LASTEXITCODE -eq 0 -or $out -notmatch "No PC Torchlight found.") { Fail "list: $out" }
One "gog" (Join-Path $Fixture "prefix\drive_c\GOG Games\Torchlight") $GogProxy "360" @()
$backups = @(Get-ChildItem -LiteralPath (Split-Path $saves) -Directory | Where-Object { $_.Name -like "save-backup-*" })
if ($backups.Count -lt 1) { Fail "gog: no save backup" }
foreach ($b in $backups) { if ((Hash (Join-Path $b.FullName "0.svt")) -ne (Hash (Join-Path $saves "0.svt"))) { Fail "gog: a backup differs" } }
One "steam" (Join-Path $Fixture "steam\Torchlight") $SteamProxy "pc" @("--experimental")
Write-Output "check_release: OK"
