#!/bin/bash

if [[ -z $BANAN_ROOT_DIR ]]; then
	export BANAN_ROOT_DIR="$(realpath $(dirname $(realpath "$0"))/..)"
fi

source "$BANAN_ROOT_DIR/script/config.sh"

export BANAN_XBPS_REPO="$BANAN_PORT_DIR/package/repo"

set -u

package_standalone() {
	local version_string="$(../get-version-string.sh)"

	local xbps_file="$BANAN_XBPS_REPO/${version_string}.xbps"
	if [[ -f "$xbps_file" ]]; then
		[[ 'build.sh' -ot "$xbps_file" ]] && [[ ! -d 'patches' || 'patches' -ot "$xbps_file" ]] && return
		rm "$xbps_file"
	fi

	local dependency
	for dependency in $(source build.sh; echo ${DEPENDENCIES[*]}); do
		pushd "$BANAN_PORT_DIR/$dependency" >/dev/null
		package_standalone
		popd >/dev/null
	done

	rm -rf "$BANAN_BUILD_DIR"
	rm -rf "$version_string"
	rm -rf "$BANAN_PORT_DIR/package/$version_string"

	PACKAGE=1 ./build.sh || echo "$version_string" >> ../failed-ports
}

cd "$BANAN_PORT_DIR"
for build_script in */build.sh; do
	port_name="${build_script%/*}"

	[[ "$port_name" == 'llvm' ]] && continue

	pushd "$BANAN_PORT_DIR/$port_name" >/dev/null
	package_standalone
	popd >/dev/null
done
