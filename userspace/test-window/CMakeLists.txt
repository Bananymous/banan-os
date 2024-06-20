set(SOURCES
	main.cpp
)

add_executable(test-window ${SOURCES})
banan_include_headers(test-window ban)
banan_include_headers(test-window libinput)
banan_link_library(test-window libc)
banan_link_library(test-window libgui)

install(TARGETS test-window OPTIONAL)
