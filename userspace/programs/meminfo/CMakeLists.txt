set(SOURCES
	main.cpp
)

add_executable(meminfo ${SOURCES})
banan_link_library(meminfo libc)

install(TARGETS meminfo OPTIONAL)
