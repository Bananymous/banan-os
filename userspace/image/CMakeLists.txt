set(SOURCES
	main.cpp
)

add_executable(image ${SOURCES})
banan_include_headers(image ban)
banan_link_library(image libc)
banan_link_library(image libimage)

install(TARGETS image OPTIONAL)
