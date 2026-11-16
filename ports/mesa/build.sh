#!/bin/bash ../install.sh

NAME='mesa'
VERSION='26.2.7'
DOWNLOAD_URL="https://archive.mesa3d.org/mesa-$VERSION.tar.xz#04294f75d22e9f61cd1e050206bc48b3d39366e788f1803a3368e3d5a872ce18"
SPLIT_PACKAGES=('mesa-common' 'mesa-osmesa' 'mesa-glx')
DEPENDENCIES=(
	'expat'
	'libX11'
	'libXext'
	'libXfixes'
	'libXrandr'
	'libxshmfence'
	'libXxf86vm'
	'zlib'
	'zstd'
)
BUILD_DEPENDENCIES=('llvm')
CONFIGURE_OPTIONS=(
	'-Dprefix=/usr'
	'-Dbuildtype=release'
	'-Dshared-llvm=disabled'
	'-Dplatforms=x11'
	'-Dvulkan-drivers='
	'-Dgallium-drivers=llvmpipe'
	'-Dglx=dri'
	'-Dosmesa=true'
)

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
				'/usr/include/GL/glx.h'
				'/usr/include/GL/glxext.h'
				'/usr/include/GL/internal/dri_interface.h'
				'/usr/lib/dri/libdril_dri.so'
				'/usr/lib/dri/swrast_dri.so'
				'/usr/lib/libEGL.so.1.0.0'
				'/usr/lib/libEGL.so.1'
				'/usr/lib/libEGL.so'
				"/usr/lib/libgallium-$VERSION.so"
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
