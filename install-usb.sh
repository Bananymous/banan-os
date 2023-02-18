#!/bin/sh
set -e
. ./disk.sh

SIZE=$(stat -c '%s' banan-os.img | numfmt --to=iec)

echo Writing ${SIZE}B
sudo dd if=banan-os.img of=/dev/sda status=progress
