set(SOURCES
	main.cpp
)

add_executable(touch ${SOURCES})
banan_link_library(touch libc)

install(TARGETS touch OPTIONAL)
