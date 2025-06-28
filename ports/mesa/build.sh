#!/bin/bash ../install.sh

NAME='mesa'
VERSION='25.0.7'
DOWNLOAD_URL="https://archive.mesa3d.org/mesa-$VERSION.tar.xz#592272df3cf01e85e7db300c449df5061092574d099da275d19e97ef0510f8a6"
DEPENDENCIES=('zlib' 'zstd' 'expat')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dosmesa=true'
	'-Dgallium-drivers=softpipe'
	'-Dvulkan-drivers=[]'
	'-Dplatforms=[]'
	'-Dglx=disabled'
	'-Dbuildtype=release'
)

configure() {
	meson setup --reconfigure --cross-file "$MESON_CROSS_FILE" "${CONFIGURE_OPTIONS[@]}" build || exit 1
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$BANAN_SYSROOT" -C build || exit 1

	ln -sf osmesa.pc $BANAN_SYSROOT/usr/lib/pkgconfig/opengl.pc
	ln -sf libOSMesa.so $BANAN_SYSROOT/usr/lib/libGL.so
}
