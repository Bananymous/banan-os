if [[ -z $BANAN_ROOT_DIR ]]; then
	if ! [[ -z $BANAN_SCRIPT_DIR ]]; then
		export BANAN_ROOT_DIR="$(realpath $BANAN_SCRIPT_DIR/..)"
	else
		echo  "You must set the BANAN_ROOT_DIR environment variable" >&2
		exit 1
	fi
fi

if [[ -z $BANAN_ARCH ]]; then
	export BANAN_ARCH='x86_64'
fi

export BANAN_TOOLCHAIN_DIR="$BANAN_ROOT_DIR/toolchain"
export BANAN_TOOLCHAIN_PREFIX="$BANAN_TOOLCHAIN_DIR/local"
export BANAN_TOOLCHAIN_TRIPLE="$BANAN_ARCH-pc-banan_os"

export BANAN_BUILD_DIR="$BANAN_ROOT_DIR/build"

export BANAN_PORT_DIR="$BANAN_ROOT_DIR/ports"

export BANAN_SYSROOT="$BANAN_BUILD_DIR/sysroot"
export BANAN_SYSROOT_TAR="$BANAN_SYSROOT.tar"

export BANAN_DISK_IMAGE_PATH="$BANAN_BUILD_DIR/banan-os.img"

if [[ -z $BANAN_UEFI_BOOT ]]; then
	export BANAN_UEFI_BOOT=0
fi

if [[ -z $BANAN_BOOTLOADER ]]; then
	export BANAN_BOOTLOADER='BANAN'
fi

export BANAN_CMAKE="$BANAN_TOOLCHAIN_PREFIX/bin/cmake"
