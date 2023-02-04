#!/bin/sh
set -e
. ./disk.sh

SIZE=$(stat banan-os.img | grep -oP '\d+' | head -n 1)

echo Writing $SIZE bytes
sudo dd if=banan-os.img of=/dev/sda status=progress
