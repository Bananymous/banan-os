#!/bin/bash

cd $(dirname $(realpath $0))

for port in ./*/.compile_hash*; do
	pushd $(dirname "$port") >/dev/null
	./build.sh
	popd >/dev/null
done
