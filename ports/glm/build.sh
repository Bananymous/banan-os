#!/bin/bash ../install.sh

NAME='glm'
VERSION='1.0.2'
DOWNLOAD_URL="https://github.com/g-truc/glm/archive/refs/tags/$VERSION.tar.gz#19edf2e860297efab1c74950e6076bf4dad9de483826bc95e2e0f2c758a43f65"

configure() {
	cmake --fresh -B build -S . -G Ninja \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		-DGLM_BUILD_TESTS=OFF \
		|| exit 1
}

build() {
	cmake --build build || exit 1
}

install() {
	cmake --install build || exit 1
}
