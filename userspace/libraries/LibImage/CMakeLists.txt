set(LIBIMAGE_SOURCES
	Image.cpp
	Netbpm.cpp
	PNG.cpp
)

add_library(libimage ${LIBIMAGE_SOURCES})
banan_link_library(libimage ban)
banan_link_library(libimage libc)

banan_install_headers(libimage)
install(TARGETS libimage OPTIONAL)
