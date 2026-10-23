#!/bin/sh
# Downloads, checked by SHA-256, the source the AppImage runtime's LGPL-2.1 part asks to go with the
# release (packaging/linux/APPIMAGE_NOTICES.md): libfuse 3.15.0, and type2-runtime 8f39b89, whose
# patch to libfuse and build scripts make the runtime from it. The release workflow attaches both.
# Usage: get_sources.sh DIR
set -eu
[ $# -eq 1 ] || { echo "usage: $0 DIR" >&2; exit 2; }
mkdir -p "$1"
cd "$1"
get() {  # NAME URL SHA256
  curl -fsSL -o "$1" "$2" || { echo "get_sources: cannot download $1 from $2" >&2; exit 1; }
  echo "$3  $1" | sha256sum -c - > /dev/null || { echo "get_sources: $1 does not match" >&2; exit 1; }
}
get fuse-3.15.0.tar.xz \
  https://github.com/libfuse/libfuse/releases/download/fuse-3.15.0/fuse-3.15.0.tar.xz \
  70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0
get type2-runtime-8f39b89.tar.gz \
  https://github.com/AppImage/type2-runtime/archive/8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa.tar.gz \
  c449eb42c2190820872631f5e22f8e15b1e985a36dc4a9e2de3585bb7536c785
