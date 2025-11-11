#!/bin/bash

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
fi

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
	sed -i "s|CMAKE|$BANAN_CMAKE|" "$MESON_CROSS_FILE"
	sed -i "s|SYSROOT|$BANAN_SYSROOT|" "$MESON_CROSS_FILE"
fi

MAKE_BUILD_TARGETS=('all')
MAKE_INSTALL_TARGETS=('install')

CONFIG_SUB=()

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
		make $target "DESTDIR=$BANAN_SYSROOT" || exit 1
	done
}

source $1

if [ -z $NAME ] || [ -z $VERSION ] || [ -z $DOWNLOAD_URL ]; then
	echo  "Port does not set needed environment variables" >&2
	exit 1
fi

build_dir="$NAME-$VERSION-$BANAN_ARCH"

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

	pushd "../$dependency" >/dev/null
	pwd
	if ! ./build.sh; then
		echo "Failed to install dependency '$dependency' of port '$NAME'"
		exit 1
	fi
	popd >/dev/null
done

if [ "$VERSION" = "git" ]; then
	regex="(.*/.*\.git)#(.*)"

	if [[ $DOWNLOAD_URL =~ $regex ]]; then
		REPO_URL="${BASH_REMATCH[1]}"
		COMMIT="${BASH_REMATCH[2]}"

		if [ -d "$build_dir" ]; then
			pushd $build_dir >/dev/null
			if [ ! "$(git rev-parse HEAD)" = "$COMMIT" ]; then
				clean
			fi
			popd >/dev/null
		fi

		if [ ! -d "$build_dir" ]; then
			git clone "$REPO_URL" "$build_dir" || exit 1

			pushd "$build_dir" >/dev/null
			git checkout "$COMMIT" || exit 1
			if [ -d ../patches ]; then
				for patch in ../patches/*.patch ; do
					git apply "$patch" || exit 1
				done
			fi
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

			if [ -d patches ]; then
				for patch in ./patches/*.patch ; do
					patch -ruN -p1 -d "$build_dir" < "$patch" || exit 1
				done
			fi
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

pre_install
install
grep -qsxF "$NAME-$VERSION" "$installed_file" || echo "$NAME-$VERSION" >> "$installed_file"
post_install
