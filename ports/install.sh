#!/bin/bash

shopt -s nullglob

if (( $# != 1 )); then
	echo "No arguments given for $0" >&2
	exit 1
fi

if [[ -z $BANAN_ROOT_DIR ]]; then
	export BANAN_ROOT_DIR="$(realpath $(dirname $(realpath $0))/..)"
fi

source "$BANAN_ROOT_DIR/script/config.sh"

installed_file="$BANAN_PORT_DIR/.installed_ports"
if [ -z $DONT_REMOVE_INSTALLED ]; then
	export DONT_REMOVE_INSTALLED=1
	rm -f "$installed_file"
	rm -rf "$BANAN_SYSROOT/var/db/xbps"
fi

LOCAL_PACKAGE_ROOT="$BANAN_PORT_DIR/package"
LOCAL_PACKAGE_REPO="$LOCAL_PACKAGE_ROOT/repo"

export PATH="$BANAN_TOOLCHAIN_PREFIX/bin:$PATH"

export PKG_CONFIG_DIR=''
export PKG_CONFIG_SYSROOT_DIR="$BANAN_SYSROOT"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/usr/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_SYSROOT_DIR/usr/share/pkgconfig"

export CC="$BANAN_TOOLCHAIN_TRIPLE-gcc"
export CXX="$BANAN_TOOLCHAIN_TRIPLE-g++"
export LD="$BANAN_TOOLCHAIN_TRIPLE-ld"
export AR="$BANAN_TOOLCHAIN_TRIPLE-ar"
export RANLIB="$BANAN_TOOLCHAIN_TRIPLE-ranlib"
export READELF="$BANAN_TOOLCHAIN_TRIPLE-readelf"
export OBJCOPY="$BANAN_TOOLCHAIN_TRIPLE-objcopy"
export OBJDUMP="$BANAN_TOOLCHAIN_TRIPLE-objdump"
export STRIP="$BANAN_TOOLCHAIN_TRIPLE-strip"
export CXXFILT="$BANAN_TOOLCHAIN_TRIPLE-c++filt"

if [ "$BANAN_ARCH" = "i686" ]; then
	export LDFLAGS="-shared-libgcc"
fi

export MESON_CROSS_FILE="$BANAN_PORT_DIR/$BANAN_ARCH-banan_os-meson.txt"
if [ ! -f "$MESON_CROSS_FILE" ] || [ "$MESON_CROSS_FILE" -ot "$BANAN_TOOLCHAIN_DIR/meson-cross-file.in" ]; then
	cp "$BANAN_TOOLCHAIN_DIR/meson-cross-file.in" "$MESON_CROSS_FILE"
	sed -i "s|ARCH|$BANAN_ARCH|" "$MESON_CROSS_FILE"
	sed -i "s|SYSROOT|$BANAN_SYSROOT|" "$MESON_CROSS_FILE"
	sed -i "s|PKG_CONFIG|$BANAN_PORT_DIR/pkg-config|" "$MESON_CROSS_FILE"
	sed -i "s|CMAKE_BINARY|$BANAN_CMAKE|" "$MESON_CROSS_FILE"
	sed -i "s|CMAKE_TOOLCHAIN|$BANAN_TOOLCHAIN_DIR/Toolchain.txt|" "$MESON_CROSS_FILE"
fi

MAKE_BUILD_TARGETS=('all')
MAKE_INSTALL_TARGETS=('install')

CONFIG_SUB=()

REVISION=1

clean() {
	find . -mindepth 1 -maxdepth 1 -not -name 'patches' -not -name 'build.sh' -exec rm -rf {} +
}

config_sub_update() {
	[ ${#CONFIG_SUB[@]} -eq 0 ] && return

	config_sub_path="$BANAN_PORT_DIR/config.sub"

	if [ ! -f "$config_sub_path" ]; then
		wget -O "$config_sub_path.tmp" 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD' || exit 1
		mv $config_sub_path.tmp $config_sub_path
	fi

	for target in "${CONFIG_SUB[@]}"; do
		cp "$config_sub_path" "$target"
	done
}

pre_configure() {
	:
}

post_configure() {
	:
}

configure() {
	configure_options=("--host=$BANAN_ARCH-pc-banan_os" '--prefix=/usr')
	configure_options+=("${CONFIGURE_OPTIONS[@]}")
	./configure "${configure_options[@]}" || exit 1
}

pre_build() {
	:
}

post_build() {
	:
}

build() {
	for target in "${MAKE_BUILD_TARGETS[@]}"; do
		make -j$(nproc) $target || exit 1
	done
}

pre_install() {
	:
}

post_install() {
	:
}

install() {
	for target in "${MAKE_INSTALL_TARGETS[@]}"; do
		make $target "DESTDIR=$DESTDIR" || exit 1
	done
}

source "$1"

if [ -z $DESCRIPTION ]; then
	DESCRIPTION="TODO description for $NAME"
fi

if [ -z $NAME ] || [ -z $VERSION ] || [ -z $DOWNLOAD_URL ]; then
	echo  "Port does not set needed environment variables" >&2
	exit 1
fi

build_dir="$(../get-version-string.sh)"

if [ ! -d "$build_dir" ]; then
	rm -f '.compile_hash'
fi

if [ ! -f '.compile_hash' ] && [ -f "$installed_file" ]; then
	sed -i "/^$NAME-$VERSION$/d" "$installed_file"
fi

if grep -qsxF "$NAME-$VERSION" "$installed_file"; then
	exit 0
fi

pushd "$BANAN_ROOT_DIR" >/dev/null
./bos all && ./bos install || exit 1
popd >/dev/null

for dependency in "${DEPENDENCIES[@]}"; do
	if [ ! -d "../$dependency" ]; then
		echo "Could not find dependency '$dependency' or port '$NAME'"
		exit 1
	fi

	if [ -z "$NO_XBPS_DEPS" ]; then
		if run_xbps xbps-install \
			-R "$BANAN_XBPS_REPO" \
			-r "$BANAN_SYSROOT" \
			-Siy "$dependency"
		then
			(cd "../$dependency" && ../get-version-string.sh) >> "$installed_file"
			continue
		fi
	fi

	if ! (cd "../$dependency" && ./build.sh); then
		echo "Failed to install dependency '$dependency' of port '$NAME'"
		exit 1
	fi
done

if [ "$VERSION" = "git" ]; then
	regex="(.*/.*\.git)#(.*)"

	if [[ $DOWNLOAD_URL =~ $regex ]]; then
		REPO_URL="${BASH_REMATCH[1]}"
		COMMIT="${BASH_REMATCH[2]}"

		if [ -d "$build_dir" ]; then
			if [ ! "$(cd "$build_dir" && git rev-parse HEAD)" = "$COMMIT" ]; then
				rm -r "$build_dir"
			fi
		fi

		if [ ! -d "$build_dir" ]; then
			git clone --depth=1 --revision="$COMMIT" "$REPO_URL" "$build_dir" || exit 1

			pushd "$build_dir" >/dev/null
			for patch in ../patches/*.patch ; do
				git apply "$patch" || exit 1
			done
			popd >/dev/null
		fi
	else
		echo "Invalid format in DOWNLOAD_URL" >&2
		exit 1
	fi
else
	regex='.*/(.*)#(.*)'
	if [[ $DOWNLOAD_URL =~ $regex ]]; then
		FILE_NAME="${BASH_REMATCH[1]}"
		FILE_HASH="${BASH_REMATCH[2]}"

		if [ -f "$FILE_NAME" ]; then
			if ! echo "$FILE_HASH $FILE_NAME" | sha256sum --check >/dev/null; then
				clean
			fi
		fi

		if [ ! -f "$FILE_NAME" ]; then
			wget "$DOWNLOAD_URL" || exit 1
		fi

		if ! echo "$FILE_HASH $FILE_NAME" | sha256sum --check >/dev/null; then
			echo "File hash does not match" >&2
			exit 1
		fi

		regex='(.*\.tar\..*|.*\.tgz)'
		if [[ $FILE_NAME =~ $regex ]] && [ ! -d "$build_dir" ]; then
			tar xf "$FILE_NAME" || exit 1

			: "${TAR_CONTENT:=$NAME-$VERSION}"
			mv "$TAR_CONTENT" "$build_dir" || exit 1

			for patch in ./patches/*.patch ; do
				patch -ruN -p1 -d "$build_dir" < "$patch" || exit 1
			done
		else
			if [ ! -d "$build_dir" ]; then
				mkdir -p "$build_dir" || exit 1
			fi
		fi
	else
		echo "Invalid format in DOWNLOAD_URL" >&2
		exit 1
	fi
fi

needs_compile=1
if [ -f ".compile_hash" ]; then
	sha256sum --check ".compile_hash" &>/dev/null
	needs_compile=$?
fi

cd "$build_dir"

if (( $needs_compile )); then
	config_sub_update

	pre_configure
	configure
	post_configure

	pre_build
	build
	post_build

	sha256sum "$BANAN_SYSROOT/usr/lib/libc.a" > "../.compile_hash"
fi

DESTDIR="$BANAN_SYSROOT"
pre_install
install
post_install

test -d "$BANAN_SYSROOT/usr/lib"     && find "$BANAN_SYSROOT/usr/lib"     -name '*.la' -delete
test -d "$BANAN_SYSROOT/usr/libexec" && find "$BANAN_SYSROOT/usr/libexec" -name '*.la' -delete
rm -f "$BANAN_SYSROOT/usr/share/info/dir"

grep -qsxF "$NAME-$VERSION" "$installed_file" || echo "$NAME-$VERSION" >> "$installed_file"

if (( $PACKAGE )); then
	mkdir -p "$LOCAL_PACKAGE_ROOT" || exit 1

	version_string=$(cd .. && ../get-version-string.sh)

	package_dir="$LOCAL_PACKAGE_ROOT/$version_string"

	rm -rf "$package_dir"
	mkdir "$package_dir"

	dependencies=''
	for dep in "${DEPENDENCIES[@]}"; do
		dependencies+="$dep>=0 "
	done

	DESTDIR="$package_dir"
	pre_install
	install
	post_install

	test -d "$DESTDIR/usr/lib"     && find "$DESTDIR/usr/lib"     -name '*.la' -delete
	test -d "$DESTDIR/usr/libexec" && find "$DESTDIR/usr/libexec" -name '*.la' -delete
	rm -f "$DESTDIR/usr/share/info/dir"

	mkdir -p "$LOCAL_PACKAGE_REPO" || exit 1
	cd "$LOCAL_PACKAGE_REPO" || exit 1

	run_xbps xbps-create -A "$BANAN_ARCH" -n "${version_string%.*}" -s "$DESCRIPTION" -D "$dependencies" "$package_dir"
	run_xbps xbps-rindex -af "$version_string.xbps"
fi
