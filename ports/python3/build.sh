#!/bin/bash ../install.sh

NAME='python'
VERSION='3.14.7'
DOWNLOAD_URL="https://www.python.org/ftp/python/$VERSION/Python-$VERSION.tar.xz#3b48dac8fb59f62eaa67ac83c1eb12bda1b7a08406dd286e252c11a66be27f81"
TAR_CONTENT="Python-$VERSION"
DEPENDENCIES=('ncurses' 'openssl' 'libffi' 'zlib' 'zstd' 'bzip2' 'xz' 'sqlite3')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	"--build=$(uname -m)-pc-linux-gnu"
	'--with-build-python=python3.14'
	'--with-ensurepip=install'
	'--disable-ipv6'
	'--enable-shared'
	'--enable-loadable-sqlite-extensions'
	'--disable-test-modules'
	'ac_cv_file__dev_ptmx=no'
	'ac_cv_file__dev_ptc=no'
)

post_install() {
	mkdir -p "$DESTDIR/usr/bin"
	ln -svf python3 "$DESTDIR/usr/bin/python" || exit 1
}
