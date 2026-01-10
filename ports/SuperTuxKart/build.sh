#!/bin/bash ../install.sh

NAME='SuperTuxKart'
VERSION='1.5'
DOWNLOAD_URL="https://github.com/supertuxkart/stk-code/releases/download/$VERSION/SuperTuxKart-$VERSION-src.tar.gz#33cf8841e4ff4082d80b9248014295bbbea61d14683e86dff100e3ab8f7b27cb"
TAR_CONTENT="SuperTuxKart-$VERSION-src"
DEPENDENCIES=('SDL2' 'curl' 'openal-soft' 'freetype' 'harfbuzz' 'libvorbis' 'libjpeg' 'libpng' 'zlib')

configure() {
	return
	$BANAN_CMAKE -B build -S . -G Ninja --fresh \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_RECORDER=OFF \
		-DUSE_WIIUSE=OFF \
		-DUSE_DNS_C=ON \
    	-DUSE_IPV6=OFF \
		-DNO_SHADERC=ON \
		|| exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
}
