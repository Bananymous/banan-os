set(SOURCES
	main.cpp
)

add_executable(test-uinput ${SOURCES})
banan_include_headers(test-uinput libinput)
banan_link_library(test-uinput libc)

install(TARGETS test-uinput OPTIONAL)
