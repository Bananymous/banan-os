#!/bin/bash ../install.sh

NAME='xeyes'
VERSION='1.3.1'
DOWNLOAD_URL="https://www.x.org/releases/individual/app/xeyes-$VERSION.tar.xz#5608d76b7b1aac5ed7f22f1b6b5ad74ef98c8693220f32b4b87dccee4a956eaa"
DEPENDENCIES=('libXi' 'libX11' 'libXt' 'libXext' 'libXmu' 'libXrender')
CONFIG_SUB=('config.sub')
