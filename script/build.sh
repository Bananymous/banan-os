#!/bin/bash
set -e

export BANAN_SCRIPT_DIR=$(dirname $(realpath $0))
source $BANAN_SCRIPT_DIR/config.sh

run_fakeroot() {
	if [ ! -f $BANAN_FAKEROOT ]; then
		touch $BANAN_FAKEROOT
	fi
	fakeroot -i $BANAN_FAKEROOT -s $BANAN_FAKEROOT -- /bin/bash -c '$@' bash $@
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
	run_fakeroot $BANAN_CMAKE --build $BANAN_BUILD_DIR -- -j$(nproc) $1
}

build_toolchain () {
	$BANAN_TOOLCHAIN_DIR/build.sh
}

build_tools() {
	perm_tool="$BANAN_TOOLS_DIR/update-image-perms"
	if [ ! -f "$perm_tool" ] || [ "$perm_tool" -ot "$perm_tool.c" ]; then
		gcc -O2 -Wall -Wextra -Werror -o "$perm_tool" "$perm_tool.c" || exit 1
	fi
}

create_image () {
	build_target all
	build_target install
	build_tools
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

if [ ! -d $BANAN_SYSROOT ]; then
	mkdir -p $BANAN_SYSROOT

	pushd $BANAN_SYSROOT
	run_fakeroot tar xf ${BANAN_ROOT_DIR}/base-sysroot.tar.gz
	popd
fi

if [ -v QEMU_ACCEL ]; then
	:
elif type kvm-ok &> /dev/null; then
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
		run_qemu -serial stdio -d int -action reboot=shutdown,shutdown=pause
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
		rm -f $BANAN_SYSROOT.tar
		rm -f $BANAN_DISK_IMAGE_PATH
		;;
	distclean)
		rm -rf $BANAN_BUILD_DIR
		find $BANAN_ROOT_DIR/ports -name '.compile_hash' -exec rm {} +
		;;
	*)
		build_target $1
		;;
esac
