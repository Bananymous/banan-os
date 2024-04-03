#!/bin/sh

CURRENT_DIR=$(dirname $(realpath $0))
pushd "$CURRENT_DIR" >/dev/null

while read port; do
	${port}/build.sh
done < installed

popd >/dev/null
