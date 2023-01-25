#!/bin/sh
set -e
. ./build.sh
 
mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub
 
cp sysroot/boot/banan-os.kernel isodir/boot/banan-os.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "banan-os" {
	multiboot /boot/banan-os.kernel
}
menuentry "banan-os (no serial)" {
	multiboot /boot/banan-os.kernel noserial
}
menuentry "banan-os (no apic)" {
	multiboot /boot/banan-os.kernel noapic
}
menuentry "banan-os (no apic, no serial)" {
	multiboot /boot/banan-os.kernel noapic noserial
}
EOF
grub-mkrescue -o banan-os.iso isodir
