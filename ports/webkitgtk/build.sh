#!/bin/bash ../install.sh

NAME='webkitgtk'
VERSION='2.54.2'
DOWNLOAD_URL="https://webkitgtk.org/releases/webkitgtk-$VERSION.tar.xz#a76345b391bef6211fb5ab6cfab2e86b285d18cda17cbb9ade595f1252b86065"
DEPENDENCIES=(
	'cairo'
	'glib-networking'
	'glib'
	'gstreamer'
	'gtk3'
	'harfbuzz'
	'hyphen'
	'icu'
	'lcms2'
	'libatk'
	'libavif'
	'libbacktrace'
	'libepoxy'
	'libgcrypt'
	'libjpeg-turbo'
	'libjxl'
	'libpng'
	'libsoup'
	'libtasn1'
	'libwebp'
	'libxml2'
	'sqlite3'
	'woff2'
	'zlib'
)

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DPORT=GTK -DUSE_GTK4=OFF \
		-DENABLE_DOCUMENTATION=OFF \
		-DENABLE_GAMEPAD=OFF \
		-DENABLE_INTROSPECTION=OFF \
		-DENABLE_JOURNALD_LOG=OFF \
		-DENABLE_SPEECH_SYNTHESIS=OFF \
		-DENABLE_SPELLCHECK=OFF \
		-DENABLE_WEBGL=OFF \
		-DENABLE_XSLT=OFF \
		-DUSE_GBM=OFF \
		-DUSE_LIBDRM=OFF \
		-DUSE_LIBSECRET=OFF \
		-DUSE_SYSPROF_CAPTURE=OFF \
		-DUSE_SYSTEM_MALLOC=ON \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	DESTDIR="$DESTDIR" cmake --install build || exit 1
}

post_install() {
	mkdir -p "$DESTDIR/etc/profile.d"
	echo 'export WEBKIT_DISABLE_DMABUF_RENDERER=1' > "$DESTDIR/etc/profile.d/webkitgtk.sh"

	mkdir -p "$DESTDIR/usr/bin"
	ln -svf ../libexec/webkit2gtk-4.1/MiniBrowser "$DESTDIR/usr/bin/webkitgtk"
}
