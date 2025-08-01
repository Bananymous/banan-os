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

if [ "$1" == "full" ] || [ ! -f $BANAN_DISK_IMAGE_PATH ]; then
	$BANAN_SCRIPT_DIR/image-create.sh || exit 1
fi

set -u

MOUNT_DIR="$BANAN_BUILD_DIR/mount"
mkdir -p $MOUNT_DIR

LOOP_DEV="$(sudo losetup --show -Pf $BANAN_DISK_IMAGE_PATH || exit 1)"
ROOT_PARTITION="${LOOP_DEV}p2"
if [ ! -b $ROOT_PARTITION ]; then
	echo "Failed to probe partitions for banan disk image." >&2
	sudo losetup -d $LOOP_DEV
	exit 1
fi

if sudo mount $ROOT_PARTITION $MOUNT_DIR; then
	if (($BANAN_INITRD)); then
		fakeroot -i $BANAN_FAKEROOT tar -C $BANAN_SYSROOT -cf $BANAN_SYSROOT.initrd .

		sudo mkdir -p $MOUNT_DIR/boot
		sudo cp $BANAN_BUILD_DIR/kernel/banan-os.kernel $MOUNT_DIR/boot/banan-os.kernel
		sudo mv $BANAN_SYSROOT.initrd $MOUNT_DIR/boot/banan-os.initrd
	else
		sudo rsync -rulHpt $BANAN_SYSROOT/ $MOUNT_DIR

		fakeroot -i $BANAN_FAKEROOT find $BANAN_SYSROOT -printf './%P|%U|%G\n' >$BANAN_BUILD_DIR/sysroot-perms.txt
		sudo bash -c "
			if enable stat &>/dev/null; then
				while IFS='|' read -r path uid gid; do
					full=\"$MOUNT_DIR/\$path\"
					stat \"\$full\"
					if [[ \${STAT[uid]} != \$uid ]] || [[ \${STAT[gid]} != \$gid ]]; then
						chown -h \"\$uid:\$gid\" \"\$full\"
					fi
				done <$BANAN_BUILD_DIR/sysroot-perms.txt
			else
				while IFS='|' read -r path uid gid; do
					full=\"$MOUNT_DIR/\$path\"
					if [[ \$(stat -c '%u %g' \"\$full\") != \"\$uid \$gid\" ]]; then
						chown -h \"\$uid:\$gid\" \"\$full\"
					fi
				done <$BANAN_BUILD_DIR/sysroot-perms.txt
			fi
		"
	fi

	sudo umount $MOUNT_DIR
fi

sudo losetup -d $LOOP_DEV
