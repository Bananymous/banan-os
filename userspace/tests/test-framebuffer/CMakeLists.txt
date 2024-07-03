set(SOURCES
	main.cpp
)

add_executable(test-framebuffer ${SOURCES})
banan_link_library(test-framebuffer libc)

install(TARGETS test-framebuffer OPTIONAL)
