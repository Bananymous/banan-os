set(SOURCES
	main.cpp
)

add_executable(init ${SOURCES})
banan_include_headers(init ban)
banan_link_library(init libc)

install(TARGETS init)
