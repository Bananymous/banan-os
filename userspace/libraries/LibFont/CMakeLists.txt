set(LIBGUI_SOURCES
	Font.cpp
	PSF.cpp
)

add_library(libfont ${LIBGUI_SOURCES})
banan_link_library(libfont ban)
banan_link_library(libfont libc)

banan_install_headers(libfont)
install(TARGETS libfont OPTIONAL)
