#!/bin/bash

cd $(dirname $(realpath $0))

if [ ! -f installed ]; then
	exit 0
fi

while read port; do
	${port}/build.sh
done < installed
