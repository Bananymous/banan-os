set(LIBINPUT_SOURCES
	KeyEvent.cpp
	KeyboardLayout.cpp
)

add_library(libinput ${LIBINPUT_SOURCES})
banan_include_headers(libinput ban)
banan_link_library(libinput libc)

banan_install_headers(libinput)
install(TARGETS libinput OPTIONAL)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
