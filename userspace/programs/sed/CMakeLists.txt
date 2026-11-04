set(SOURCES
	main.cpp
)

add_executable(sed ${SOURCES})
banan_link_library(sed libc)

install(TARGETS sed OPTIONAL)
