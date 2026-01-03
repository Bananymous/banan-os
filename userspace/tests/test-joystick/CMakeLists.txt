set(SOURCES
	main.cpp
)

add_executable(test-joystick ${SOURCES})
banan_include_headers(test-joystick ban)
banan_include_headers(test-joystick libinput)
banan_link_library(test-joystick libc)

install(TARGETS test-joystick OPTIONAL)
