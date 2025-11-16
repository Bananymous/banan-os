#!/bin/bash ../install.sh

NAME='SDL_mixer'
VERSION='1.2.12'
DOWNLOAD_URL="https://github.com/libsdl-org/SDL_mixer/archive/refs/tags/release-$VERSION.tar.gz#4176dfc887664419bfd16c41013c6cf0c48eca6b95ae3c34205630e8a7a94faa"
TAR_CONTENT="SDL_mixer-release-$VERSION"
CONFIG_SUB=('build-scripts/config.sub')
DEPENDENCIES=('libmikmod' 'libiconv' 'sdl12-compat')
