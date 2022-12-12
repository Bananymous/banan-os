#!/bin/sh
set -e
. ./iso.sh
 
sudo su -c "cat banan-os.iso > /dev/sda"
 