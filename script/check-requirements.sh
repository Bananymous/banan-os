#!/bin/bash

CMAKE_VERSION_REQUIRED="3.26"

version_atleast() {
	[ "$1" = "$(echo -e "$1\n$2" | sort -rV | head -n1)" ]
}

download_cmake() {
	read -e -p "Do you want to download it [y/N]? " choice
	if ! [[ "$choice" == [Yy]* ]]; then
		echo "Build requirements not met" >&2
		return 1
	fi

	CMAKE_FULL_NAME="cmake-3.26.6-linux-x86_64"

	mkdir -p $BANAN_BUILD_DIR/toolchain
	mkdir -p $BANAN_TOOLCHAIN_PREFIX/bin
	mkdir -p $BANAN_TOOLCHAIN_PREFIX/share
	cd $BANAN_BUILD_DIR/toolchain

	if ! [[ -f $CMAKE_FULL_NAME.tar.gz ]]; then
		wget https://cmake.org/files/v3.26/$CMAKE_FULL_NAME.tar.gz
	fi

	if ! [[ -d $CMAKE_FULL_NAME ]]; then
		tar xf $CMAKE_FULL_NAME.tar.gz
	fi

	cp -r $CMAKE_FULL_NAME/bin/* $BANAN_TOOLCHAIN_PREFIX/bin/
	cp -r $CMAKE_FULL_NAME/share/* $BANAN_TOOLCHAIN_PREFIX/share/

	export CMAKE_COMMAND="$BANAN_TOOLCHAIN_PREFIX/bin/cmake"
}

if ! type ninja &> /dev/null ; then
	echo "ninja not found" >&2
	return 1
fi

if [ -z "$CMAKE_COMMAND" ]; then
	if [ -f $BANAN_TOOLCHAIN_PREFIX/bin/cmake ]; then
		export CMAKE_COMMAND="$BANAN_TOOLCHAIN_PREFIX/bin/cmake"
	else
		export CMAKE_COMMAND=cmake
	fi
fi

if ! type $CMAKE_COMMAND &> /dev/null ; then
	echo "You don't seem to have cmake installed"
	download_cmake
	return 0
fi

CMAKE_VERSION=$($CMAKE_COMMAND --version | head -n1 | cut -d' ' -f3)
if ! version_atleast "$CMAKE_VERSION" "$CMAKE_VERSION_REQUIRED" ; then
	echo "Your cmake version ($CMAKE_VERSION) is less than the required $CMAKE_VERSION_REQUIRED"
	download_cmake
	return 0
fi
