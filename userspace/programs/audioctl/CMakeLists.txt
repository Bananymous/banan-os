set(SOURCES
	main.cpp
)

add_executable(audioctl ${SOURCES})
banan_link_library(audioctl ban)
banan_link_library(audioctl libc)
banan_link_library(audioctl libaudio)

install(TARGETS audioctl OPTIONAL)
