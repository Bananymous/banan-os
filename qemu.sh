#!/bin/sh
set -e
. ./iso.sh
 
qemu-system-$(./target-triplet-to-arch.sh $HOST) \
	-m 128												\
	-drive format=raw,media=cdrom,file=banan-os.iso		\
	-serial stdio										\
 