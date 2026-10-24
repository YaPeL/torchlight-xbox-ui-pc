#!/bin/bash
# Runs the GOG Torchlight copy through Proton's own script, without Steam, in a prefix of its own
# (docs/xbox-ui-on-pc.md section 0). Never Steam's compatdata/41500.
#
#   run_gog.sh [extra arguments for Torchlight.exe]
#
# The locations below are this machine's; change them here if they move.
set -euo pipefail
STEAM_ROOT="$HOME/.steam/debian-installation"
PROTON="$STEAM_ROOT/steamapps/common/Proton - Experimental/proton"
GAME_DIR="$HOME/torchlight-gog/prefix/drive_c/GOG Games/Torchlight"
COMPAT="$HOME/torchlight-gog/compat"
LOGS="$HOME/torchlight-gog"

case "$COMPAT" in *compatdata/41500*) echo "refusing: that is Steam's Torchlight prefix"; exit 1;; esac
# The saves the run will use, backed up and checked first; any doubt stops the run (save_guard.py).
python3 "$(dirname "$0")/save_guard.py" backup "$COMPAT" "$LOGS/save-backups" --search "$LOGS"
if pgrep -x steam > /dev/null; then echo "note: Steam is running (harmless for GOG's executable, which has no steam_api)"; fi
mkdir -p "$COMPAT"
cd "$GAME_DIR"
# SteamGameId/SteamAppId: without an id the script exits at once with code 1 and writes no log.
# winemenubuilder off: no shortcuts or file associations on the desktop.
exec env STEAM_COMPAT_DATA_PATH="$COMPAT" STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT" \
  SteamGameId=gogtl SteamAppId=0 WINEDLLOVERRIDES="winemenubuilder.exe=d" \
  PROTON_LOG=1 PROTON_LOG_DIR="$LOGS" \
  "$PROTON" run "$GAME_DIR/Torchlight.exe" "$@"
