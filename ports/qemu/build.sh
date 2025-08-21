#!/bin/bash ../install.sh

NAME='qemu'
VERSION='10.0.2'
DOWNLOAD_URL="https://download.qemu.org/qemu-$VERSION.tar.xz#ef786f2398cb5184600f69aef4d5d691efd44576a3cff4126d38d4c6fec87759"
DEPENDENCIES=('glib' 'SDL2')
MAKE_BUILD_TARGETS=('qemu-system-x86_64')
CONFIGURE_OPTIONS=(
	'--cross-prefix='
	'--target-list=x86_64-softmmu'
	'--disable-tpm'
	'--disable-docs'
)

pre_configure() {
	echo '' > tests/meson.build
}
