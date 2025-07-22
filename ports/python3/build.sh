#!/bin/bash ../install.sh

NAME='python'
VERSION='3.13.3'
DOWNLOAD_URL="https://www.python.org/ftp/python/$VERSION/Python-$VERSION.tar.xz#40f868bcbdeb8149a3149580bb9bfd407b3321cd48f0be631af955ac92c0e041"
TAR_CONTENT="Python-$VERSION"
DEPENDENCIES=('ncurses' 'zlib' 'openssl')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	"--build=$(uname -m)-pc-linux-gnu"
	"--with-build-python=python3.13"
	'--without-ensurepip'
	'--disable-ipv6'
	'--disable-test-modules'
	'--enable-shared'
	'ac_cv_file__dev_ptmx=no'
	'ac_cv_file__dev_ptc=no'
)

pre_configure() {
	if ! command -v python3.13 &>/dev/null ; then
		echo "You need to have python3.13 installed on your host machine" >&2
		exit 1
	fi
}
