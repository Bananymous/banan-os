#!/bin/sh
set -e
. ./iso.sh
 
cat > bochsrc << EOF
ata0-master: type=cdrom, path=banan-os.iso, status=inserted
boot: cdrom
clock: sync=realtime, time0=local
display_library: x, options="gui_debug"
magic_break: enabled=1
megs: 128
com1: enabled=1, mode=term, dev=/dev/pts/2
EOF

bochs -q
