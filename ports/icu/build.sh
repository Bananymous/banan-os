#!/bin/bash ../install.sh

NAME='icu'
VERSION='78.1'
DOWNLOAD_URL="https://github.com/unicode-org/icu/releases/download/release-$VERSION/icu4c-$VERSION-sources.tgz#6217f58ca39b23127605cfc6c7e0d3475fe4b0d63157011383d716cb41617886"
TAR_CONTENT='icu'
CONFIG_SUB=('source/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-tests'
	'--disable-samples'
)

pre_configure() {
	unset CC CXX LD AS AR

	mkdir -p source/build-host || exit 1
	pushd source/build-host || exit 1
	CONFIGURE_OPTIONS+=("--with-cross-build=$(realpath .)")
	../configure --disable-tests --disable-samples || exit 1
	make -j$(proc) || exit 1
	popd >/dev/null || exit 1

	pushd source >/dev/null || exit 1
}

post_configure() {
	popd >/dev/null || exit 1
}

pre_build() {
	pushd source >/dev/null || exit 1
}

post_build() {
	popd >/dev/null || exit 1
}

pre_install() {
	pushd source >/dev/null || exit 1
}

post_install() {
	popd >/dev/null || exit 1
}
