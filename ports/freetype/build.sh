#!/bin/bash ../install.sh

NAME='freetype'
VERSION='2.13.3'
DOWNLOAD_URL="https://download.savannah.gnu.org/releases/freetype/freetype-$VERSION.tar.gz#5c3a8e78f7b24c20b25b54ee575d6daa40007a5f4eea2845861c3409b3021747"
CONFIG_SUB=('builds/unix/config.sub')

CONFIGURE_OPTIONS=(
	'lt_cv_deplibs_check_method=pass_all'
)

post_install() {
	# remove libtool file
	rm -f $BANAN_SYSROOT/usr/lib/libfreetype.la
}
