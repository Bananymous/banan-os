#!/bin/bash ../install.sh

NAME='xclock'
VERSION='1.1.1'
DOWNLOAD_URL="https://www.x.org/releases/individual/app/xclock-$VERSION.tar.xz#df7ceabf8f07044a2fde4924d794554996811640a45de40cb12c2cf1f90f742c"
DEPENDENCIES=('libXaw' 'libXmu' 'libX11' 'libXft' 'libxkbfile')
CONFIG_SUB=('config.sub')
