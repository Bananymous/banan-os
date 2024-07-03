set(SOURCES
	main.cpp
)

add_executable(test-globals ${SOURCES})
banan_link_library(test-globals libc)

install(TARGETS test-globals OPTIONAL)
