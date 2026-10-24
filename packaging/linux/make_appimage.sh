#!/bin/sh
# Makes the installer's AppImage: the executable (built with libstdc++ and libgcc inside it, so it
# needs only the system's libc), a desktop file, an icon and the notices, with the pinned runtime
# get_tools.sh downloads.
# Usage: make_appimage.sh TOOLS_DIR EXECUTABLE OUTPUT.AppImage
set -eu
[ $# -eq 3 ] || { echo "usage: $0 TOOLS_DIR EXECUTABLE OUTPUT.AppImage" >&2; exit 2; }
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
tools=$(cd "$1" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
appdir="$work/AppDir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/doc/x360ui-install"
cp "$2" "$appdir/usr/bin/x360ui-install"
chmod +x "$appdir/usr/bin/x360ui-install"
cp "$here/x360ui-install.desktop" "$appdir/"
cp "$repo/installer/data/icon/x360ui-install.png" "$appdir/x360ui-install.png"   # the icon (its README: source, licence)
cp "$repo/LICENSE" "$repo/installer/third_party/NOTICES.md" "$repo/installer/third_party/LGPL-2.1.txt" "$here/APPIMAGE_NOTICES.md" \
   "$appdir/usr/share/doc/x360ui-install/"
cat > "$appdir/AppRun" <<'RUN'
#!/bin/sh
here=$(dirname "$(readlink -f "$0")")
exec "$here/usr/bin/x360ui-install" "$@"
RUN
chmod +x "$appdir/AppRun"
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 "$tools/appimagetool" --no-appstream \
  --runtime-file "$tools/runtime-x86_64" "$appdir" "$3"
echo "AppImage: $3"
