#!/bin/bash ../install.sh

NAME='quake2'
VERSION='git'
DOWNLOAD_URL="https://github.com/ozkl/quake2generic.git#50190797664fd42fc1b0266150c54f76f92bfa15"
DEPENDENCIES=('SDL2' 'SDL2_mixer')

configure() {
	make clean
}

build() {
	baseq2_tar=../baseq2.tar.gz
	baseq2_hash=9660c306d9440ff7d534f165ae4a7f550b9e879d5190830953034ebda10e873a

	if [ -f $baseq2_tar ]; then
		if ! echo "$baseq2_hash $baseq2_tar" | sha256sum --check >/dev/null; then
			rm $baseq2_tar
		fi
	fi
	if [ ! -f $baseq2_tar ]; then
		wget https://bananymous.com/files/baseq2.tar.gz -O $baseq2_tar || exit 1
	fi
	if ! echo "$baseq2_hash $baseq2_tar" | sha256sum --check >/dev/null; then
		echo "File hash does not match" >&2
		exit 1
	fi

	cflags='-Dstricmp=strcasecmp -Wno-incompatible-pointer-types'
	make CC="$CC" BASE_CFLAGS="$cflags" SDL_PATH="$BANAN_SYSROOT/usr/bin/" -j$(nproc) || exit 1
}

install() {
	cp -v build/quake2-soft "${BANAN_SYSROOT}/bin/quake2" || exit 1

	baseq2_tar=$(realpath ../baseq2.tar.gz || exit 1)
	cd "$BANAN_SYSROOT/home/user/"
	tar xf $baseq2_tar
}
