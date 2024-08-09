#!/bin/bash

if [ -z $BANAN_PORT_DIR ]; then
	echo  "You must set the BANAN_PORT_DIR environment variable" >&2
	exit 1
fi

cd $(dirname $(realpath $0))

rm -f $BANAN_PORT_DIR/.installed

while IFS= read -r port; do
	pushd $(dirname "$port") >/dev/null
	./build.sh
	popd >/dev/null
done < <(find $BANAN_PORT_DIR -name '.compile_hash')
