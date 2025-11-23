set(SOURCES
	main.cpp
)

add_executable(cp ${SOURCES})
banan_link_library(cp libc)

install(TARGETS cp OPTIONAL)
