set(SOURCES
	main.cpp
	AudioServer.cpp
)

add_executable(AudioServer ${SOURCES})
banan_link_library(AudioServer ban)
banan_link_library(AudioServer libc)
banan_link_library(AudioServer libaudio)

install(TARGETS AudioServer OPTIONAL)
