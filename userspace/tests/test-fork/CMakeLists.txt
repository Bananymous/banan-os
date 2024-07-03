set(SOURCES
	main.cpp
)

add_executable(test-fork ${SOURCES})
banan_link_library(test-fork libc)

install(TARGETS test-fork OPTIONAL)
