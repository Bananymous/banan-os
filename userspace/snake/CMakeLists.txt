set(SOURCES
	main.cpp
)

add_executable(snake ${SOURCES})
banan_include_headers(snake ban)
banan_link_library(snake libc)

install(TARGETS snake)
