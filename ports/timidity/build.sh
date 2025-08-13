#!/bin/bash ../install.sh

NAME='timidity'
VERSION='2.15.0'
DOWNLOAD_URL="https://sourceforge.net/projects/timidity/files/TiMidity++/TiMidity++-$VERSION/TiMidity++-$VERSION.tar.gz#0b6109a3c64604c8851cd9bb4cbafc014a4e13b0025f597e586d9742388f6fb7"
TAR_CONTENT="TiMidity++-$VERSION"
CONFIG_SUB=('autoconf/config.sub')
CONFIGURE_OPTIONS=(
	'--without-x'
	'lib_cv_va_copy=y'
	'lib_cv___va_copy=n'
	'lib_cv_va_val_copy=n'
	'CFLAGS=-std=c11'
)

post_install() {
	if [ ! -f ../eawpats.zip ]; then
		wget https://www.quaddicted.com/files/idgames/sounds/eawpats.zip -O ../eawpats.zip || exit 1
	fi

	eawpats_dir="/usr/share/eawpats"
	mkdir -p "$BANAN_SYSROOT/$eawpats_dir"
	unzip -qod "$BANAN_SYSROOT/$eawpats_dir" ../eawpats.zip

	cp "$BANAN_SYSROOT/$eawpats_dir/timidity.cfg" "$BANAN_SYSROOT/etc/"
	sed -i "s|^dir .*$|dir $eawpats_dir|g" "$BANAN_SYSROOT/etc/timidity.cfg"
}
