set(LIBGUI_SOURCES
	Font.cpp
	PSF.cpp
)

add_library(libfont ${LIBGUI_SOURCES})
banan_link_library(libfont libc)
banan_link_library(libfont ban)

banan_install_headers(libfont)
install(TARGETS libfont OPTIONAL)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
