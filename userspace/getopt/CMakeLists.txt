set(SOURCES
	main.cpp
)

add_executable(getopt ${SOURCES})
banan_include_headers(getopt ban)
banan_link_library(getopt libc)

install(TARGETS getopt)
