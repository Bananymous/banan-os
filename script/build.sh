#!/bin/bash
set -e

export BANAN_SCRIPT_DIR=$(dirname $(realpath $0))
source $BANAN_SCRIPT_DIR/config.sh

FAKEROOT_FILE="$BANAN_BUILD_DIR/fakeroot-context"

run_fakeroot() {
	fakeroot -i $FAKEROOT_FILE -s $FAKEROOT_FILE -- /bin/bash -c '$@' bash $@
}

make_build_dir () {
	mkdir -p $BANAN_BUILD_DIR
	cd $BANAN_BUILD_DIR
	if ! [[ -f "build.ninja" ]]; then
		cmake --toolchain=$BANAN_TOOLCHAIN_DIR/Toolchain.txt -G Ninja $BANAN_ROOT_DIR
	fi
}

build_target () {
	make_build_dir
	if [[ $# -eq 0 ]]; then
		echo "No target provided"
		exit 1
	fi
	cd $BANAN_BUILD_DIR
	run_fakeroot ninja $1
}

build_toolchain () {
	if [[ -f $BANAN_TOOLCHAIN_PREFIX/bin/$BANAN_TOOLCHAIN_TRIPLE_PREFIX-gcc ]]; then
		echo "You already seem to have a toolchain."
		read -e -p "Do you want to rebuild it [y/N]? " choice
		if ! [[ "$choice" == [Yy]* ]]; then
			echo "Aborting toolchain rebuild"
			exit 0
		fi
	fi
	
	$BANAN_TOOLCHAIN_DIR/build.sh
	build_target libc-install
	$BANAN_TOOLCHAIN_DIR/build.sh libstdc++
}

create_image () {
	build_target install-sysroot
	$BANAN_SCRIPT_DIR/image.sh "$1"
}

run_qemu () {
	create_image
	$BANAN_SCRIPT_DIR/qemu.sh $@
}

run_bochs () {
	create_image
	$BANAN_SCRIPT_DIR/bochs.sh $@
}

if [[ -c /dev/kvm ]]; then
	QEMU_ACCEL="-accel kvm"
fi

if [[ $# -eq 0 ]]; then
	echo "No argument given"
	exit 1
fi

case $1 in
	toolchain)
		build_toolchain
		;;
	image)
		create_image
		;;
	image-full)
		create_image full
		;;
	qemu)
		run_qemu -serial stdio $QEMU_ACCEL
		;;
	qemu-nographic)
		run_qemu -nographic $QEMU_ACCEL
		;;
	qemu-debug)
		run_qemu -serial stdio -d int -no-reboot
		;;
	bochs)
		run_bochs
		;;
	check-fs)
		$BANAN_SCRIPT_DIR/check-fs.sh
		;;
	clean)
		build_target clean
		rm -f $FAKEROOT_FILE
		rm -rf $BANAN_SYSROOT
		;;
	bootloader)
		create_image
		build_target bootloader
		$BANAN_ROOT_DIR/bootloader/install.sh
		$BANAN_SCRIPT_DIR/qemu.sh -serial stdio $QEMU_ACCEL
		;;
	*)
		build_target $1
		;;
esac
