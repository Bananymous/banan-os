#!/bin/bash ../install.sh

NAME='freetype'
VERSION='2.14.1'
DOWNLOAD_URL="https://download.savannah.gnu.org/releases/freetype/freetype-$VERSION.tar.xz#32427e8c471ac095853212a37aef816c60b42052d4d9e48230bab3bdf2936ccc"
DEPENDENCIES=('zlib' 'libpng')
CONFIG_SUB=('builds/unix/config.sub')
CONFIGURE_OPTIONS=(
	'lt_cv_deplibs_check_method=pass_all'
)
