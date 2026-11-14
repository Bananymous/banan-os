#!/bin/bash
set -e

export BANAN_SCRIPT_DIR=$(dirname $(realpath $0))
source "$BANAN_SCRIPT_DIR/config.sh"

run_fakeroot() {
	touch "$BANAN_FAKEROOT"
	fakeroot -i "$BANAN_FAKEROOT" -s "$BANAN_FAKEROOT" -- "$@"
}

build_target () {
	if [ ! -f "$BANAN_CMAKE" ]; then
		echo "cmake not found, please re-run toolchain compilation script"
		exit 1
	fi

	if [ $# -eq 0 ]; then
		echo "No target provided"
		exit 1
	fi

	cd "$BANAN_ROOT_DIR"

	if [ ! -d "$BANAN_BUILD_DIR/CMakeFiles" ]; then
		"$BANAN_CMAKE" --toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" -B "$BANAN_BUILD_DIR"
	fi

	if [ "$1" != 'install' ]; then
		"$BANAN_CMAKE" --build "$BANAN_BUILD_DIR" --parallel --target "$1"
	else
		run_fakeroot "$BANAN_CMAKE" --install "$BANAN_BUILD_DIR"
	fi
}

build_toolchain () {
	"$BANAN_TOOLCHAIN_DIR/build.sh"
}

build_tools() {
	pushd "$BANAN_TOOLS_DIR/ext2-writer" &>/dev/null
	if [ ! -d 'build' ]; then
		"$BANAN_CMAKE" -B build || exit 1
	fi
	"$BANAN_CMAKE" --build build --parallel || exit 1
	popd &>/dev/null
}

create_image () {
	build_target all
	build_target install
	build_tools
	"$BANAN_SCRIPT_DIR/image.sh" "$1"
}

create_iso () {
	build_target all
	build_target install
	"$BANAN_SCRIPT_DIR/iso.sh"
}

run_qemu () {
	create_image
	"$BANAN_SCRIPT_DIR/qemu.sh" "$@"
}

run_bochs () {
	create_image
	"$BANAN_SCRIPT_DIR/bochs.sh" "$@"
}

if [ ! -d "$BANAN_SYSROOT" ]; then
	mkdir -p "$BANAN_SYSROOT"

	pushd "$BANAN_SYSROOT"
	run_fakeroot tar xf "$BANAN_ROOT_DIR/base-sysroot.tar.gz"
	popd
fi

if [ -v QEMU_ACCEL ]; then
	:
elif type kvm-ok &> /dev/null; then
	if kvm-ok &> /dev/null; then
		QEMU_ACCEL="-accel kvm"
	fi
elif [ -c /dev/kvm ]; then
	if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
		QEMU_ACCEL="-accel kvm"
	else
		echo "You don't have read/write permissions for /dev/kvm" >&2
	fi
fi

if [ -z "$CMAKE_GENERATOR" ] && type ninja &>/dev/null; then
	export CMAKE_GENERATOR='Ninja'
fi

if [ $# -eq 0 ]; then
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
	iso)
		create_iso
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
	xbps-*)
		command="$1"
		shift

		build_target all
		build_target install

		xbps_args=()

		case "$command" in
			xbps-install|xbps-remove|xbps-query)
				xbps_args+=(--rootdir "$BANAN_SYSROOT")
				;;
		esac

		case "$command" in
			xbps-install|xbps-query)
				xbps_args+=(--repository "$BANAN_XBPS_REPO")
				;;
		esac

		run_xbps "$command" "${xbps_args[@]}" "$@"
		;;
	*)
		build_target $1
		;;
esac
