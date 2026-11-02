#!/bin/bash ../install.sh

NAME='libdrm'
VERSION='2.4.134'
DOWNLOAD_URL="https://dri.freedesktop.org/libdrm/libdrm-$VERSION.tar.xz#ac5e74d157830eb8bee44c6a6bf3ad49774ef0dd2a72bdad74a8f20308b52a95"
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dintel=disabled'
	'-Dradeon=disabled'
	'-Damdgpu=disabled'
	'-Dnouveau=disabled'
	'-Dvmwgfx=disabled'
	'-Domap=disabled'
	'-Dexynos=disabled'
	'-Dfreedreno=disabled'
	'-Dtegra=disabled'
	'-Dvc4=disabled'
	'-Detnaviv=disabled'
	'-Dtests=false'
)

configure() {
	meson setup --reconfigure --cross-file "$MESON_CROSS_FILE" "${CONFIGURE_OPTIONS[@]}" build || exit 1
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$DESTDIR" -C build || exit 1
}
