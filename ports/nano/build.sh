#!/bin/bash ../install.sh

NAME='nano'
VERSION='8.5'
DOWNLOAD_URL="https://www.nano-editor.org/dist/v8/nano-$VERSION.tar.xz#000b011d339c141af9646d43288f54325ff5c6e8d39d6e482b787bbc6654c26a"
DEPENDENCIES=('ncurses')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'ac_cv_header_glob_h=no'
)

pre_configure() {
	echo '#include_next <sys/types.h>' > lib/sys_types.in.h
}
