#!/bin/bash ../install.sh

NAME='tuxracer'
VERSION='0.61'
DOWNLOAD_URL="http://download.sourceforge.net/tuxracer/tuxracer-$VERSION.tar.gz#a311d09080598fe556134d4b9faed7dc0c2ed956ebb10d062e5d4df022f91eff"
DEPENDENCIES=('tcl' 'mesa' 'glu' 'sdl12-compat')
CONFIGURE_OPTIONS=(
	'--with-data-dir=/usr/share/tuxracer'
	"--with-sdl-prefix=$BANAN_SYSROOT/usr"
	'--without-gl-libs'
	'--with-tcl-lib-name=tcl8.6'
	'--without-x'
)

post_install() {
	pushd ..

	if [ ! -f "tuxracer-data-$VERSION.tar.gz" ]; then
		wget "http://download.sourceforge.net/tuxracer/tuxracer-data-$VERSION.tar.gz" || exit 1
	fi

	if [ ! -d "tuxracer-data-$VERSION" ]; then
		tar xf "tuxracer-data-$VERSION.tar.gz" || exit 1
	fi

	mkdir -p "$BANAN_SYSROOT/usr/share/tuxracer" || exit 1
	cp -r "tuxracer-data-$VERSION"/* "$BANAN_SYSROOT/usr/share/tuxracer/" || exit 1
	find "$BANAN_SYSROOT/usr/share/tuxracer" -type f -exec chmod 644 {} +

	popd
}

export CFLAGS="-std=c99 -Wno-implicit-int -Wno-incompatible-pointer-types $CFLAGS"
export ac_cv_func_isnan=yes
