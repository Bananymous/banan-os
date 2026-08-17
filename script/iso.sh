#!/bin/bash

if [ -z $BANAN_ISO_PATH ]; then
	echo  "You must set the BANAN_ISO_PATH environment variable" >&2
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

set -u

iso_dir="$BANAN_BUILD_DIR/iso"

rm -rf "$iso_dir"
mkdir -p "$iso_dir/boot/grub"

echo "Packing initrd"
fakeroot -i "$BANAN_FAKEROOT" tar -C "$BANAN_SYSROOT" --exclude='./boot' -zcf "$iso_dir/boot/banan-os.initrd" .

strip -o "$iso_dir/boot/banan-os.kernel" --strip-unneeded "$BANAN_BUILD_DIR/kernel/banan-os.kernel"

cat > "$iso_dir/boot/grub/grub.cfg" << EOF
insmod all_video
menuentry "banan-os" {
	multiboot2 /boot/banan-os.kernel readonly
	module2 --nounzip /boot/banan-os.initrd
}
EOF

echo "Creating ISO"
grub-mkrescue --compress=gz -o "$BANAN_ISO_PATH" "$iso_dir" >/dev/null
