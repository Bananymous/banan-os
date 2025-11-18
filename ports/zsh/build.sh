#!/bin/bash ../install.sh

NAME='zsh'
VERSION='5.9'
DOWNLOAD_URL="https://sourceforge.net/projects/zsh/files/zsh/$VERSION/zsh-$VERSION.tar.xz#9b8d1ecedd5b5e81fbf1918e876752a7dd948e05c1a0dba10ab863842d45acd5"
DEPENDENCIES=('ncurses')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'ac_cv_func_mmap_fixed_mapped=yes'
	'zsh_cv_sys_path_dev_fd=no'
	'ac_cv_have_dev_ptmx=no'
	'CFLAGS=-Wno-incompatible-pointer-types'
)
