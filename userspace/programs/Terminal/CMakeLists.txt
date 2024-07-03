set(SOURCES
	main.cpp
	Terminal.cpp
)

add_executable(Terminal ${SOURCES})
banan_include_headers(Terminal ban)
banan_link_library(Terminal libc)
banan_link_library(Terminal libfont)
banan_link_library(Terminal libgui)
banan_link_library(Terminal libinput)

install(TARGETS Terminal OPTIONAL)
