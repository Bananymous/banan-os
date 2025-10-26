set(LIBQR_SOURCES
	QRCode.cpp
)

add_library(libqr ${LIBQR_SOURCES})
banan_link_library(libqr ban)
banan_link_library(libqr libc)

banan_install_headers(libqr)
install(TARGETS libqr OPTIONAL)
