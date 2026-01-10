#!/bin/bash ../install.sh

NAME='SuperTux'
VERSION='0.6.3'
DOWNLOAD_URL="https://github.com/SuperTux/supertux/releases/download/v$VERSION/SuperTux-v$VERSION-Source.tar.gz#f7940e6009c40226eb34ebab8ffb0e3a894892d891a07b35d0e5762dd41c79f6"
TAR_CONTENT="SuperTux-v$VERSION-Source"
DEPENDENCIES=('boost' 'SDL2' 'SDL2_image' 'curl' 'openal-soft' 'libvorbis' 'freetype' 'physfs' 'glm')

configure() {
	cmake --fresh -B build -S . -G Ninja  \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DINSTALL_SUBDIR_BIN=bin \
		-DBUILD_DOCUMENTATION=OFF \
		-DENABLE_OPENGL=OFF \
		-DENABLE_BOOST_STATIC_LIBS=ON \
		|| exit 1
	# crashes in `std::ostream::sentry::sentry(std::ostream&)` with shared boost
}

build() {
	cmake --build build || exit 1
}

install() {
	cmake --install build || exit 1
}
