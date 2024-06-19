set(LIBIMAGE_SOURCES
	Image.cpp
	Netbpm.cpp
	PNG.cpp
)

add_library(libimage ${LIBIMAGE_SOURCES})
target_compile_options(libimage PRIVATE -O3)
banan_link_library(libimage libc)
banan_link_library(libimage ban)

banan_install_headers(libimage)
install(TARGETS libimage)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
