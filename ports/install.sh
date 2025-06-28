#!/bin/bash

if (( $# != 1 )); then
	echo "No arguments given for $0" >&2
	exit 1
fi

if [[ -z $BANAN_ROOT_DIR ]]; then
	export BANAN_ROOT_DIR="$(realpath $(dirname $(realpath $0))/..)"
fi

source "$BANAN_ROOT_DIR/script/config.sh"

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

export CMAKE_TOOLCHAIN_FILE="$BANAN_TOOLCHAIN_DIR/Toolchain.txt"

pushd "$BANAN_ROOT_DIR" >/dev/null
./bos all && ./bos install || exit 1
popd >/dev/null

if [ "$BANAN_ARCH" = "i686" ]; then
	export LDFLAGS="-shared-libgcc"
fi

export MESON_CROSS_FILE="$BANAN_PORT_DIR/$BANAN_ARCH-banan_os-meson.txt"
if [ ! -f "$MESON_CROSS_FILE" ] || [ "$MESON_CROSS_FILE" -ot "$BANAN_TOOLCHAIN_DIR/meson-cross-file.in" ]; then
	cp "$BANAN_TOOLCHAIN_DIR/meson-cross-file.in" "$MESON_CROSS_FILE"
	sed -i "s/ARCH/$BANAN_ARCH/" "$MESON_CROSS_FILE"
	sed -i "s/SYSROOT/$BANAN_SYSROOT/" "$MESON_CROSS_FILE"
fi

clean() {
	find . -mindepth 1 -maxdepth 1 -not -name 'patches' -not -name 'build.sh' -exec rm -rf {} +
}

pre_configure() {
	:
}

post_configure() {
	:
}

configure() {
	pre_configure

	configure_options=("--host=$BANAN_ARCH-pc-banan_os" '--prefix=/usr')
	configure_options+=("${CONFIGURE_OPTIONS[@]}")
	./configure "${configure_options[@]}" || exit 1

	post_configure
}

build() {
	make -j$(nproc) || exit 1
}

install() {
	make install "DESTDIR=$BANAN_SYSROOT" || exit 1
}

source $1

if [ -z $NAME ] || [ -z $VERSION ] || [ -z $DOWNLOAD_URL ]; then
	echo  "Port does not set needed environment variables" >&2
	exit 1
fi

for dependency in "${DEPENDENCIES[@]}"; do
	pushd "../$dependency" >/dev/null
	pwd
	if ! ./build.sh; then
		echo "Failed to install dependency '$dependency' of port '$NAME'"
		exit 1
	fi
	popd >/dev/null
done

build_dir="$NAME-$VERSION-$BANAN_ARCH"

if [ ! -d "$build_dir" ]; then
	rm -f ".compile_hash"
fi

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
				for patch in ../patches/*; do
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

		regex='(.*\.tar\..*)'
		if [[ $FILE_NAME =~ $regex ]] && [ ! -d "$build_dir" ]; then
			tar xf "$FILE_NAME" || exit 1

			: "${TAR_CONTENT:=$NAME-$VERSION}"
			mv "$TAR_CONTENT" "$build_dir" || exit 1

			if [ -d patches ]; then
				for patch in ./patches/*; do
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
	configure
	build
	sha256sum "$BANAN_SYSROOT/usr/lib/libc.a" > "../.compile_hash"
fi

install
