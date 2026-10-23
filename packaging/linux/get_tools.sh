#!/bin/sh
# Downloads appimagetool and the AppImage runtime at pinned versions, checked by SHA-256, into DIR
# (make_appimage.sh passes the runtime with --runtime-file: without it appimagetool would download
# whatever type2-runtime's continuous release is at that moment).
# Usage: get_tools.sh DIR
set -eu
[ $# -eq 1 ] || { echo "usage: $0 DIR" >&2; exit 2; }
mkdir -p "$1"
cd "$1"
get() {  # NAME URL SHA256 [FALLBACK_URL]: the fallback is checked by the same hash
  if ! curl -fsSL -o "$1" "$2"; then
    [ -n "${4:-}" ] || { echo "get_tools: cannot download $1 from $2" >&2; exit 1; }
    echo "get_tools: $1 not at $2; from $4" >&2
    curl -fsSL -o "$1" "$4"
  fi
  echo "$3  $1" | sha256sum -c - > /dev/null || { echo "get_tools: $1 does not match" >&2; exit 1; }
  chmod +x "$1"
}
get appimagetool \
  https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
  ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
# type2-runtime 8f39b89 (packaging/linux/APPIMAGE_NOTICES.md). Upstream publishes it only as its
# continuous release, which each build replaces, so the same file (the same hash) is read from a
# release of YaPeL/torchlight-recomp-deps; upstream's is the fallback, which fails the check once
# upstream moves on.
get runtime-x86_64 \
  https://github.com/YaPeL/torchlight-recomp-deps/releases/download/appimage-runtime-8f39b89/runtime-x86_64 \
  156f4bdbde9c52d01814600013e0a273f0118dc2de98975f3c8c63427ec79074 \
  https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
