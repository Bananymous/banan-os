#!/bin/bash

cd $(dirname $(realpath $0))

while IFS= read -r port; do
	pushd $(dirname "$port") >/dev/null
	./build.sh
	popd >/dev/null
done < <(find . -name '.compile_hash*')
