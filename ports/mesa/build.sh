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
	llvm_dir_name="llvm-banan_os-$llvm_version-$BANAN_ARCH"

	if [ ! -d "../$llvm_dir_name" ]; then
		pushd ..

		if [ ! -f "$llvm_dir_name.tar.gz" ]; then
			wget "https://bananymous.com/files/$llvm_dir_name.tar.gz" || exit 1
		fi

		tar xf "$llvm_dir_name.tar.gz" || exit 1

		popd
	fi

	llvm_abs_path=$(realpath ../$llvm_dir_name || exit 1)
	llvm_abs_lib="$llvm_abs_path/lib"
	llvm_abs_inc="$llvm_abs_path/include"

	mkdir -p subprojects/llvm

	wrap_file='subprojects/llvm/meson.build'
	echo "project('llvm', ['cpp'])"                        >$wrap_file
	echo ""                                               >>$wrap_file
	echo "cpp = meson.get_compiler('cpp')"                >>$wrap_file
	echo ""                                               >>$wrap_file
	echo "_deps = []"                                     >>$wrap_file
	echo "_search = '$llvm_abs_lib'"                      >>$wrap_file
	echo "foreach d : ["                                  >>$wrap_file
	for path in $llvm_abs_lib/libLLVM*.a; do
		name=$(basename $path)
		echo "    '${name:3:-2}',"                        >>$wrap_file
	done
	echo "  ]"                                            >>$wrap_file
	echo "  _deps += cpp.find_library(d, dirs : _search)" >>$wrap_file
	echo "endforeach"                                     >>$wrap_file
	echo ""                                               >>$wrap_file
	echo "dep_llvm = declare_dependency("                 >>$wrap_file
	echo "  include_directories : include_directories("   >>$wrap_file
	echo "      '$llvm_abs_inc',"                         >>$wrap_file
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
