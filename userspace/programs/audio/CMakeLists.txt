set(SOURCES
	main.cpp
)

add_executable(audio ${SOURCES})
banan_link_library(audio libc)
banan_link_library(audio libaudio)

install(TARGETS audio OPTIONAL)
