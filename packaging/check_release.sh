#!/bin/sh
# Checks a release's Linux installer where there is no Python or compiler (the release workflow's
# clean ubuntu:22.04 and archlinux containers), on tests/release_fixture.py's synthetic games:
# GOG's with the 360's bag (its save backed up) and Steam's with PC's; each install
# puts the release's proxy in place, and each uninstall leaves the folder as it was (hashes).
# Usage: check_release.sh INSTALLER FIXTURE_DIR GOG_PROXY STEAM_PROXY
set -eu
[ $# -eq 4 ] || { echo "usage: $0 INSTALLER FIXTURE_DIR GOG_PROXY STEAM_PROXY" >&2; exit 2; }
inst=$(readlink -f "$1"); fx=$(readlink -f "$2"); gog_proxy=$(readlink -f "$3"); steam_proxy=$(readlink -f "$4")
export APPIMAGE_EXTRACT_AND_RUN=1
HOME="$fx/home"; export HOME
mkdir -p "$HOME"
cd "$fx"
fail() { echo "check_release: $*" >&2; exit 1; }
same() { [ "$(sha256sum < "$1")" = "$(sha256sum < "$2")" ]; }   # no cmp: diffutils may be missing
tree() {  # every folder and file (hidden ones too), with the files' hashes
  (cd "$1" && find . | LC_ALL=C sort | while read -r f; do
    if [ -f "$f" ]; then sha256sum "$f"; else echo "folder $f"; fi
  done)
}
saves="$fx/prefix/drive_c/users/me/AppData/Roaming/runic games/torchlight/save"

out=$("$inst" list 2>&1) && fail "list found a game in an empty home: $out"
echo "$out" | grep -q "No PC Torchlight found." || fail "list: $out"

one() {  # STORE GAME PROXY BAG [EXTRA]
  store=$1 game=$2 proxy=$3 bag=$4; shift 4
  before=$(tree "$game")
  "$inst" install --game "$game" --x360-dir "$fx/x360" --yes --console --bag "$bag" "$@" || fail "$store: install"
  same "$game/CEGUIBase.dll" "$proxy" || fail "$store: the installed CEGUIBase.dll is not the release's proxy"
  grep -q "^bag=$bag" "$game/x360_ui.ini" || fail "$store: no bag=$bag in the ini"
  for d in x360ui x360ui-playstation3 x360ui-playstation4 x360ui-playstation5 x360ui-nintendo x360ui-nintendo-swapped; do
    [ -f "$game/$d/x360_ui_report.json" ] || fail "$store: no $d"
  done
  "$inst" uninstall --game "$game" --yes --console --anyway || fail "$store: uninstall"
  [ "$(tree "$game")" = "$before" ] || fail "$store: the folder is not as it was after uninstalling"
  echo "check_release: $store installed and uninstalled"
}
one gog "$fx/prefix/drive_c/GOG Games/Torchlight" "$gog_proxy" 360
[ "$(cat "$saves/0.svt")" = "a synthetic save" ] || fail "gog: the save was changed"
backups=$(ls -d "$saves"-backup-* 2>/dev/null | wc -l)
[ "$backups" -ge 1 ] || fail "gog: no save backup"
for b in "$saves"-backup-*; do [ "$(cat "$b/0.svt")" = "a synthetic save" ] || fail "gog: a backup differs from the save"; done
one steam "$fx/steam/Torchlight" "$steam_proxy" pc
echo "check_release: OK"
