set(SOURCES
	main.cpp
)

add_executable(basename ${SOURCES})
banan_link_library(basename libc)

install(TARGETS basename OPTIONAL)
