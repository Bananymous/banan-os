#!/bin/bash

if (( $# != 1 )); then
	echo "No arguments given for $0" >&2
	exit 1
fi

if [[ -z $BANAN_ROOT_DIR ]]; then
	BANAN_ROOT_DIR="$(dirname $(realpath $0))/.."
fi

source "$BANAN_ROOT_DIR/script/config.sh"

export PATH="$BANAN_TOOLCHAIN_PREFIX/bin:$PATH"

export PKG_CONFIG='pkg-config --static'
export PKG_CONFIG_SYSROOT_DIR="$BANAN_SYSROOT"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/usr/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_SYSROOT_DIR/usr/share/pkgconfig"

if [ ! -f "$BANAN_SYSROOT/usr/lib/libc.a" ]; then
	pushd "$BANAN_ROOT_DIR" >/dev/null
	./bos libc || exit 1
	./bos install || exit 1
	popd >/dev/null
fi

clean() {
	find . -mindepth 1 -maxdepth 1 -not -name 'patches' -not -name 'build.sh' -exec rm -rf {} +
}

build() {
	configure_options=("--host=$BANAN_ARCH-banan_os" '--prefix=/usr')
	configure_options+=(${CONFIGURE_OPTIONS[*]})

	./configure ${configure_options[*]} || exit 1
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
	regex=".*/(.*\.tar\..*)#(.*)"
	if [[ $DOWNLOAD_URL =~ $regex ]]; then
		TAR_NAME="${BASH_REMATCH[1]}"
		TAR_HASH="${BASH_REMATCH[2]}"

		if [ -f "$TAR_NAME" ]; then
			if ! echo "$TAR_HASH $TAR_NAME" | sha256sum --check >/dev/null; then
				clean
			fi
		fi

		if [ ! -f "$TAR_NAME" ]; then
			wget "$DOWNLOAD_URL" || exit 1
		fi

		if ! echo "$TAR_HASH $TAR_NAME" | sha256sum --check >/dev/null; then
			echo "Tar hash does not match" >&2
			exit 1
		fi

		if [ ! -d "$build_dir" ]; then
			tar xf "$TAR_NAME" || exit 1

			: "${TAR_CONTENT:=$NAME-$VERSION}"
			mv "$TAR_CONTENT" "$build_dir" || exit 1

			if [ -d patches ]; then
				for patch in ./patches/*; do
					patch -ruN -p1 -d "$build_dir" < "$patch" || exit 1
				done
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
	build
	sha256sum "$BANAN_SYSROOT/usr/lib/libc.a" > "../.compile_hash"
fi

install
