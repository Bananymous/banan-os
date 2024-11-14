#!/bin/bash ../install.sh

NAME='quake2'
VERSION='git'
DOWNLOAD_URL="https://github.com/ozkl/quake2generic.git#a967e4f567a98941326fc7fe76eee5e52a04a633"

configure() {
	:
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
	make -j$(nproc) || exit 1
}

install() {
	cp build/quake2 "${BANAN_SYSROOT}/bin/" || exit 1

	baseq2_tar=$(realpath ../baseq2.tar.gz || exit 1)
	cd "$BANAN_SYSROOT/home/user/"
	tar xf $baseq2_tar
}
