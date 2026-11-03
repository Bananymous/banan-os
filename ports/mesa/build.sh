#!/bin/bash ../install.sh

NAME='mesa'
VERSION='26.1.4'
DOWNLOAD_URL="https://archive.mesa3d.org/mesa-$VERSION.tar.xz#072705caa9adf4740f1489194b13e278ad959166863b5271fe423a86353c9ab6"
SPLIT_PACKAGES=('mesa-common' 'mesa-osmesa' 'mesa-glx')
DEPENDENCIES=(
	'expat'
	'libdrm'
	'libX11'
	'libXext'
	'libXfixes'
	'libXrandr'
	'libxshmfence'
	'libXxf86vm'
	'zlib'
	'zstd'
)
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dplatforms=x11'
	'-Dvulkan-drivers='
	'-Dgallium-drivers=llvmpipe'
	'-Dglx=dri'
	'-Dosmesa=true'
)

pre_configure() {
	llvm_version='22.1.8'
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
	meson install --destdir="$DESTDIR" -C build || exit 1
}

get_split_info() {
	case "$1" in
		mesa-common)
			DEPENDENCIES=()
			WANTED_FILES=(
				'/usr/include/GL/gl.h'
				'/usr/include/GL/glcorearb.h'
				'/usr/include/GL/glext.h'
				'/usr/include/GLES/egl.h'
				'/usr/include/GLES/gl.h'
				'/usr/include/GLES/glext.h'
				'/usr/include/GLES/glplatform.h'
				'/usr/include/GLES2/gl2.h'
				'/usr/include/GLES2/gl2ext.h'
				'/usr/include/GLES2/gl2platform.h'
				'/usr/include/GLES3/gl3.h'
				'/usr/include/GLES3/gl31.h'
				'/usr/include/GLES3/gl32.h'
				'/usr/include/GLES3/gl3ext.h'
				'/usr/include/GLES3/gl3platform.h'
				'/usr/include/KHR/khrplatform.h'
				'/usr/share/drirc.d/00-mesa-defaults.conf'
			)
			;;
		mesa-osmesa)
			DEPENDENCIES=(
				'mesa-common'
				'expat'
				'libdrm'
				'zlib'
				'zstd'
			)
			WANTED_FILES=(
				'/usr/include/GL/osmesa.h'
				'/usr/lib/libOSMesa.so.8.0.0'
				'/usr/lib/libOSMesa.so.8'
				'/usr/lib/libOSMesa.so'
				'/usr/lib/pkgconfig/osmesa.pc'
			)
			;;
		mesa-glx)
			DEPENDENCIES=(
				'mesa-common'
				'expat'
				'libdrm'
				'libX11'
				'libXext'
				'libXfixes'
				'libXrandr'
				'libxshmfence'
				'libXxf86vm'
				'zlib'
				'zstd'
			)
			WANTED_FILES=(
				'/usr/include/EGL/egl.h'
				'/usr/include/EGL/eglext_angle.h'
				'/usr/include/EGL/eglext.h'
				'/usr/include/EGL/eglmesaext.h'
				'/usr/include/EGL/eglplatform.h'
				'/usr/include/gbm_backend_abi.h'
				'/usr/include/gbm.h'
				'/usr/include/GL/glx.h'
				'/usr/include/GL/glxext.h'
				'/usr/include/GL/internal/dri_interface.h'
				'/usr/lib/dri/kms_swrast_dri.so'
				'/usr/lib/dri/libdril_dri.so'
				'/usr/lib/dri/swrast_dri.so'
				'/usr/lib/gbm/dri_gbm.so'
				'/usr/lib/libEGL.so.1.0.0'
				'/usr/lib/libEGL.so.1'
				'/usr/lib/libEGL.so'
				"/usr/lib/libgallium-$VERSION.so"
				'/usr/lib/libgbm.so.1.0.0'
				'/usr/lib/libgbm.so.1'
				'/usr/lib/libgbm.so'
				'/usr/lib/libGL.so.1.2.0'
				'/usr/lib/libGL.so.1'
				'/usr/lib/libGL.so'
				'/usr/lib/libGLESv1_CM.so.1.1.0'
				'/usr/lib/libGLESv1_CM.so.1'
				'/usr/lib/libGLESv1_CM.so'
				'/usr/lib/libGLESv2.so.2.0.0'
				'/usr/lib/libGLESv2.so.2'
				'/usr/lib/libGLESv2.so'
				'/usr/lib/pkgconfig/dri.pc'
				'/usr/lib/pkgconfig/egl.pc'
				'/usr/lib/pkgconfig/gbm.pc'
				'/usr/lib/pkgconfig/gl.pc'
				'/usr/lib/pkgconfig/glesv1_cm.pc'
				'/usr/lib/pkgconfig/glesv2.pc'
				'/usr/lib/pkgconfig/glx.pc'
			)
			;;
		*)
			echo "unknown split package '$1'"
			exit 1
			;;
	esac
}
