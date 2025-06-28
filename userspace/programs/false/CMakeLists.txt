set(SOURCES
	main.cpp
)

add_executable(false ${SOURCES})
banan_link_library(false ban)
banan_link_library(false libc)

install(TARGETS false OPTIONAL)
