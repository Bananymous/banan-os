#!/bin/bash ../install.sh

NAME='libXfont2'
VERSION='2.0.7'
DOWNLOAD_URL="https://www.x.org/releases/individual/lib/libXfont2-$VERSION.tar.xz#8b7b82fdeba48769b69433e8e3fbb984a5f6bf368b0d5f47abeec49de3e58efb"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('freetype' 'xorgproto' 'xtrans' 'libfontenc')
CONFIGURE_OPTIONS=(
	'--enable-shared=yes'
	'--enable-static=no'
)
