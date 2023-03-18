#!/bin/sh
set -e
. ./disk.sh
 
qemu-system-$(./target-triplet-to-arch.sh $HOST)		\
	-m 128												\
	-smp 2												\
	-drive format=raw,media=disk,file=banan-os.img		\
	-serial stdio										\
	-accel kvm											\
 
