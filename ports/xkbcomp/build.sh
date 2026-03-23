#!/bin/bash ../install.sh

NAME='xkbcomp'
VERSION='1.4.7'
DOWNLOAD_URL="https://www.x.org/releases/individual/app/xkbcomp-$VERSION.tar.xz#0a288114e5f44e31987042c79aecff1ffad53a8154b8ec971c24a69a80f81f77"
CONFIG_SUB=('config.sub')
DEPENDENCIES=('libX11' 'libxkbfile')
