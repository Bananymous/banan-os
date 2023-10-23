#!/bin/bash
set -e

if [[ -z $BANAN_ARCH ]]; then
	export BANAN_ARCH=x86_64
fi

export BANAN_SCRIPT_DIR=$(dirname $(realpath $0))
source $BANAN_SCRIPT_DIR/config.sh

make_build_dir () {
	if ! [[ -d $BANAN_BUILD_DIR ]]; then
		mkdir -p $BANAN_BUILD_DIR
		cd $BANAN_BUILD_DIR
		cmake --toolchain=$BANAN_TOOLCHAIN_DIR/Toolchain.txt -G Ninja $BANAN_ROOT_DIR $BANAN_CMAKE_ARGS
	fi
}

build_target () {
	make_build_dir
	if [[ -z $1 ]]; then
		echo "No target provided"
		exit 1
	fi
	cd $BANAN_BUILD_DIR
	ninja $1
}

build_toolchain () {
	$BANAN_TOOLCHAIN_DIR/build.sh
	build_target libc-install
	$BANAN_TOOLCHAIN_DIR/build.sh libstdc++
}

create_image () {
	build_target install-sysroot
	if [[ "$1" == "full" ]]; then
		$BANAN_SCRIPT_DIR/image-full.sh
	else
		$BANAN_SCRIPT_DIR/image.sh
	fi
}

run_qemu () {
	create_image
	$BANAN_SCRIPT_DIR/qemu.sh $@
}

run_bochs () {
	create_image
	$BANAN_SCRIPT_DIR/bochs.sh $@
}

if [[ "$1" == "toolchain" ]]; then
	if [[ -f $BANAN_TOOLCHAIN_PREFIX/bin/$BANAN_TOOLCHAIN_TRIPLE_PREFIX-gcc ]]; then
		echo "You already seem to have build toolchain."
		read -e -p "Do you want to rebuild it [y/N]? " choice
		if ! [[ "$choice" == [Yy]* ]]; then
			echo "Aborting toolchain rebuild"
			exit 0
		fi
	fi

	build_toolchain
	exit 0
fi

if [[ "$1" == "image" ]]; then
	create_image
	exit 0
fi

if [[ "$1" == "image-full" ]]; then
	create_image full
	exit 0
fi

if [[ "$(uname)" == "Linux" ]]; then
	QEMU_ACCEL="-accel kvm"
fi

if [[ "$1" == "qemu" ]]; then
	run_qemu -serial stdio $QEMU_ACCEL
	exit 0
fi

if [[ "$1" == "qemu-nographic" ]]; then
	run_qemu -nographic $QEMU_ACCEL
	exit 0
fi

if [[ "$1" == "qemu-debug" ]]; then
	run_qemu -serial stdio -d int -no-reboot
	exit 0
fi

if [[ "$1" == "bochs" ]]; then
	run_bochs
	exit 0
fi

if [[ "$1" == "check-fs" ]]; then
	$BANAN_SCRIPT_DIR/check-fs.sh
	exit 0
fi

build_target $1
