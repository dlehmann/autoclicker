#!/usr/bin/env bash
# Builds an AppImage that bundles all Qt dependencies.
#
# Requirements (Debian 13 / trixie):
#   build-essential cmake qt6-base-dev qt6-base-private-dev qt6-wayland
#   qt6-tools-dev qt6-l10n-tools qt6-translations-l10n
#   liblayershellqtinterface-dev layer-shell-qt libgl-dev file wget ca-certificates
#
# Usage: VERSION=1.2.3 packaging/build-appimage.sh
# Result: Autoclicker-<VERSION>-x86_64.AppImage in the project directory
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT=$PWD
VERSION=${VERSION:-dev}
ARCH=x86_64
BUILD=$ROOT/build-appimage
APPDIR=$BUILD/AppDir
TOOLS=$BUILD/tools

# Run the AppImage tools without FUSE (e.g. in containers)
export APPIMAGE_EXTRACT_AND_RUN=1

rm -rf "$APPDIR"
mkdir -p "$TOOLS"

cmake -S "$ROOT" -B "$BUILD/cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD/cmake" -j"$(nproc)"
DESTDIR="$APPDIR" cmake --install "$BUILD/cmake"

fetch() {
    [ -x "$TOOLS/$1" ] || { wget -q -O "$TOOLS/$1" "$2" && chmod +x "$TOOLS/$1"; }
}
fetch linuxdeploy-$ARCH.AppImage \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
fetch linuxdeploy-plugin-qt-$ARCH.AppImage \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage"

# Qt plugin: locate Qt 6 and ship Wayland in addition to X11
export QMAKE=${QMAKE:-/usr/lib/qt6/bin/qmake}
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so"
export EXTRA_QT_MODULES="waylandclient"
export LINUXDEPLOY_OUTPUT_VERSION=$VERSION
export LDAI_OUTPUT="Autoclicker-$VERSION-$ARCH.AppImage"

# LayerShellQt ships its own Wayland shell plugin that Qt loads at runtime –
# copy it into the AppDir explicitly to be safe.
QT_PLUGINS=$("$QMAKE" -query QT_INSTALL_PLUGINS)
mkdir -p "$APPDIR/usr/plugins/wayland-shell-integration"
cp "$QT_PLUGINS/wayland-shell-integration/liblayer-shell.so" "$APPDIR/usr/plugins/wayland-shell-integration/"

cd "$BUILD"
"$TOOLS/linuxdeploy-$ARCH.AppImage" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/autoclicker" \
    --desktop-file "$APPDIR/usr/share/applications/autoclicker.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/autoclicker.svg" \
    --deploy-deps-only "$APPDIR/usr/plugins/wayland-shell-integration" \
    --plugin qt \
    --output appimage

mv "$BUILD/$LDAI_OUTPUT" "$ROOT/$LDAI_OUTPUT"
echo "Done: $ROOT/$LDAI_OUTPUT"
