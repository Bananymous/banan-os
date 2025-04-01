set(SOURCES
	main.cpp
)

add_executable(test-pthread ${SOURCES})
banan_link_library(test-pthread libc)

install(TARGETS test-pthread OPTIONAL)
