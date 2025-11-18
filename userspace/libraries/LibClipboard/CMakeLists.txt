set(LIBCLIPBOARD_SOURCES
	Clipboard.cpp
)

add_library(libclipboard ${LIBCLIPBOARD_SOURCES})
banan_link_library(libclipboard ban)
banan_link_library(libclipboard libc)

banan_install_headers(libclipboard)
install(TARGETS libclipboard OPTIONAL)
