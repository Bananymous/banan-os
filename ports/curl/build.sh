#!/bin/sh

if [ -z $BANAN_ARCH ]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_SYSROOT ]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_TOOLCHAIN_PREFIX ]; then
	echo  "You must set the BANAN_TOOLCHAIN_PREFIX environment variable" >&2
	exit 1
fi

CURL_VERSION="curl-8.8.0"
CURL_TAR="$CURL_VERSION.tar.gz"
CURL_URL="https://curl.se/download/$CURL_TAR"

cd $(dirname $(realpath $0))

if [ ! -d $CURL_VERSION ]; then
	if [ ! -f $CURL_TAR ]; then
		wget $CURL_URL
	fi
	tar xf $CURL_TAR

	for patch in ./patches/*; do
		patch -ruN -d $CURL_VERSION < "$patch"
	done

	grep -qxF doom ../installed || echo curl >> ../installed
fi

cd $CURL_VERSION

export PATH="$BANAN_TOOLCHAIN_PREFIX/bin:$PATH"

if [ ! -d "build-${BANAN_ARCH}" ]; then
	mkdir -p "build-${BANAN_ARCH}"
	cd "build-${BANAN_ARCH}"

	../configure                    \
		--host=x86_64-banan_os      \
		--prefix=$BANAN_SYSROOT/usr \
		--without-ssl               \
		--disable-threaded-resolver \
		--disable-ipv6              \
		--disable-docs

	cd ..
fi

cd "build-${BANAN_ARCH}"

make && make install
