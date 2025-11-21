#!/bin/bash ../install.sh

NAME='mesa'
VERSION='25.0.7'
DOWNLOAD_URL="https://archive.mesa3d.org/mesa-$VERSION.tar.xz#592272df3cf01e85e7db300c449df5061092574d099da275d19e97ef0510f8a6"
DEPENDENCIES=('zlib' 'zstd' 'expat')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dosmesa=true'
	'-Dvulkan-drivers=[]'
	'-Dgallium-drivers=llvmpipe'
	'-Dplatforms=[]'
	'-Dglx=disabled'
	'-Dbuildtype=release'
)

pre_configure() {
	llvm_version='20.1.8'
	llvm_root="$(realpath ../../llvm)/llvm-$llvm_version-$BANAN_ARCH"
	llvm_lib="$llvm_root/build/lib"

	if [ ! -d "$llvm_lib" ]; then
		pushd ../../llvm >/dev/null || exit 1
		./build.sh || exit 1
		popd >/dev/null
	fi

	mkdir -p subprojects/llvm

	wrap_file='subprojects/llvm/meson.build'
	echo "project('llvm', ['cpp'])"                        >$wrap_file
	echo ""                                               >>$wrap_file
	echo "cpp = meson.get_compiler('cpp')"                >>$wrap_file
	echo ""                                               >>$wrap_file
	echo "_deps = []"                                     >>$wrap_file
	echo "_search = '$llvm_lib'"                          >>$wrap_file
	echo "foreach d : ["                                  >>$wrap_file
	for path in $llvm_lib/libLLVM*.a; do
		name=$(basename $path)
		echo "    '${name:3:-2}',"                        >>$wrap_file
	done
	echo "  ]"                                            >>$wrap_file
	echo "  _deps += cpp.find_library(d, dirs : _search)" >>$wrap_file
	echo "endforeach"                                     >>$wrap_file
	echo ""                                               >>$wrap_file
	echo "dep_llvm = declare_dependency("                 >>$wrap_file
	echo "  include_directories : include_directories("   >>$wrap_file
	echo "      '$llvm_root/llvm/include',"               >>$wrap_file
	echo "      '$llvm_root/build/include',"              >>$wrap_file
	echo "    ),"                                         >>$wrap_file
	echo "  dependencies : _deps,"                        >>$wrap_file
	echo "  version : '$llvm_version',"                   >>$wrap_file
	echo ")"                                              >>$wrap_file
}

configure() {
	meson setup \
		--reconfigure \
		--cross-file "$MESON_CROSS_FILE" \
		"${CONFIGURE_OPTIONS[@]}" \
		build || exit 1
}

build() {
	meson compile -C build || exit 1
}

install() {
	meson install --destdir="$BANAN_SYSROOT" -C build || exit 1

	ln -sf osmesa.pc $BANAN_SYSROOT/usr/lib/pkgconfig/opengl.pc
	ln -sf libOSMesa.so $BANAN_SYSROOT/usr/lib/libGL.so
}
