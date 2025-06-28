set(SOURCES
	main.cpp
)

add_executable(chown ${SOURCES})
banan_link_library(chown ban)
banan_link_library(chown libc)

install(TARGETS chown OPTIONAL)
