#!/bin/bash

REVISION=1

source 'build.sh'

[ -n "$SPLIT_NAME" ] && NAME="$SPLIT_NAME"

[[ "$VERSION" == 'git' ]] && VERSION=0.0

echo "${NAME}-${VERSION//[_-]/.}_${REVISION}.${BANAN_ARCH}"
