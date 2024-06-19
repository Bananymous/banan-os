set(SOURCES
	main.cpp
)

add_executable(test-mouse ${SOURCES})
banan_include_headers(test-mouse ban)
banan_include_headers(test-mouse libinput)
banan_link_library(test-mouse libc)

install(TARGETS test-mouse)
