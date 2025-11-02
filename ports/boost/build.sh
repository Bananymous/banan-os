#!/bin/bash ../install.sh

NAME='boost'
VERSION='1.89.0'
DOWNLOAD_URL="https://archives.boost.io/release/$VERSION/source/boost_${VERSION//./_}.tar.gz#9de758db755e8330a01d995b0a24d09798048400ac25c03fc5ea9be364b13c93"
TAR_CONTENT="boost_${VERSION//./_}"
DEPENDENCIES=('zlib' 'zstd' 'libiconv')

configure() {
	# stacktrace fails on multiple definition of __cxa_allocate_exception because our libstdc++ is static
	./bootstrap.sh \
		--prefix="$BANAN_SYSROOT/usr" \
		--without-icu \
		--without-libraries='python,stacktrace' \
		|| exit 1
	echo "using gcc : : $CXX ;" > user-config.jam
}

build() {
	./b2 --user-config=user-config.jam toolset=gcc target-os=banan_os || exit 1
}

install() {
	./b2 --user-config=user-config.jam toolset=gcc target-os=banan_os install || exit 1
}
