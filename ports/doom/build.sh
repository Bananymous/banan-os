#!/bin/sh

if [ -z $BANAN_ARCH ]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

if [ -z $BANAN_SYSROOT ]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

cd $(dirname $(realpath $0))

if [ ! -d "doomgeneric" ]; then
	git clone https://github.com/ozkl/doomgeneric.git
	cd doomgeneric
	git checkout 613f870b6fa83ede448a247de5a2571092fa729d
	for patch in ../patches/*; do
		git am "$patch"
	done
	cd ..

	grep -qxF doom ../installed || echo doom >> ../installed
fi

if [ ! -f doom1.wad ]; then
	wget https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad
fi

make --directory doomgeneric/doomgeneric --file Makefile.banan_os -j $(nproc)
cp "doomgeneric/doomgeneric/build-${BANAN_ARCH}/doomgeneric" "${BANAN_SYSROOT}/bin/doom"
cp doom1.wad $BANAN_SYSROOT/home/user/
