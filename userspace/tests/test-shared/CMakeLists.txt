set(SOURCES
	main.cpp
)

add_executable(test-shared ${SOURCES})
banan_link_library(test-shared libc-shared)

install(TARGETS test-shared OPTIONAL)
