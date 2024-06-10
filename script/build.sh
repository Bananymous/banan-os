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
		$BANAN_CMAKE --toolchain=$BANAN_TOOLCHAIN_DIR/Toolchain.txt -G Ninja $BANAN_ROOT_DIR
	fi
}

build_target () {
	if ! [[ -f $BANAN_CMAKE ]]; then
		echo "cmake not found, please re-run toolchain compilation script"
		exit 1
	fi

	if ! type ninja &> /dev/null ; then
		echo "ninja not found" >&2
		exit 1
	fi

	make_build_dir
	if [[ $# -eq 0 ]]; then
		echo "No target provided"
		exit 1
	fi
	cd $BANAN_BUILD_DIR
	run_fakeroot ninja $1
}

build_toolchain () {
	$BANAN_TOOLCHAIN_DIR/build.sh
}

create_image () {
	build_target bootloader
	build_target install-sysroot
	$BANAN_ROOT_DIR/ports/build.sh
	build_target package-sysroot
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

if type kvm-ok &> /dev/null; then
	if kvm-ok &> /dev/null; then
		QEMU_ACCEL="-accel kvm"
	fi
elif [[ -c /dev/kvm ]]; then
	if [[ -r /dev/kvm ]] && [[ -w /dev/kvm ]]; then
		QEMU_ACCEL="-accel kvm"
	else
		echo "You don't have read/write permissions for /dev/kvm" >&2
	fi
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
		rm -f $BANAN_DISK_IMAGE_PATH
		;;
	*)
		build_target $1
		;;
esac
