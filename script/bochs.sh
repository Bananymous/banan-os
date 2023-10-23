#!/bin/bash
set -e

BOCHS_CONFIG_FILE=bochsrc
COM1_TERMINAL=kitty
COM1_DEVICE_FILE=com1-term-device

$COM1_TERMINAL sh -c "tty > $COM1_DEVICE_FILE && sleep infinity" &
COM1_TERM_PID=$(echo $!)

sleep 1

COM1_DEVICE=$(cat $COM1_DEVICE_FILE)
rm $COM1_DEVICE_FILE

cat > $BOCHS_CONFIG_FILE << EOF
ata0-master: type=disk, path=$BANAN_DISK_IMAGE_PATH, status=inserted
boot: disk
clock: sync=realtime, time0=local
display_library: x, options="gui_debug"
magic_break: enabled=1
megs: 128
com1: enabled=1, mode=term, dev=$COM1_DEVICE
EOF

bochs -qf $BOCHS_CONFIG_FILE
kill $COM1_TERM_PID
rm $BOCHS_CONFIG_FILE