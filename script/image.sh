#!/bin/bash

if [ -z $BANAN_DISK_IMAGE_PATH ]; then
	echo  "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_SYSROOT ]; then
	echo  "You must set the BANAN_SYSROOT environment variable" >&2
	exit 1
fi

if [ -z $BANAN_FAKEROOT ]; then
	echo  "You must set the BANAN_FAKEROOT environment variable" >&2
	exit 1
fi

if [ -z $BANAN_BUILD_DIR ]; then
	echo  "You must set the BANAN_BUILD_DIR environment variable" >&2
	exit 1
fi

if [ -z $BANAN_TOOLS_DIR ]; then
	echo  "You must set the BANAN_TOOLS_DIR environment variable" >&2
	exit 1
fi

if [ "$1" == "full" ] || [ ! -f "$BANAN_DISK_IMAGE_PATH" ]; then
	"$BANAN_SCRIPT_DIR/image-create.sh" || exit 1
fi

set -eu

root_bounds=($(parted "$BANAN_DISK_IMAGE_PATH" unit B print 2>/dev/null | grep root))
if (( "${#root_bounds[@]}" < 5 )); then
	echo "Unexpected parted output format: ${root_bounds[@]}" >&2
	exit 1
fi

fakeroot -i "$BANAN_FAKEROOT" "$BANAN_TOOLS_DIR"/ext2-writer/build/ext2-writer \
	"$BANAN_DISK_IMAGE_PATH" \
	"${root_bounds[1]::-1}" \
	"${root_bounds[3]::-1}" \
	"$BANAN_SYSROOT" \
	|| exit 1
