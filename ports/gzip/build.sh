#!/bin/bash ../install.sh

NAME='gzip'
VERSION='1.15'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/gzip/gzip-$VERSION.tar.xz#9aa0cc780dec156b8282844833b342ab7cb08c25d2cd9a1869cdd0df31deff48"

pre_configure() {
	echo '#include_next <sys/types.h>' > lib/sys_types.in.h
}
