#!/bin/bash ../install.sh

NAME='e2fsprogs'
VERSION='1.47.4'
DOWNLOAD_URL="https://kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v$VERSION/e2fsprogs-$VERSION.tar.xz#fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b"
DEPENDENCIES=('util-linux')
CONFIGURE=('configure')
CONFIGURE_OPTIONS=(
	'--sbindir=/usr/bin'
	'--enable-elf-shlibs'
)

pre_configure() {
	sed -i 's/RDYNAMIC="-rdynamic"/RDYNAMIC=/g' configure
}

post_install() {
	rm -f "$DESTDIR/usr/lib/lib"{blkid,com_err,e2p,ext2fs,ss}.a
}
