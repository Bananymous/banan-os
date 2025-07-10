set(LIBAUDIO_SOURCES
	Audio.cpp
	AudioLoader.cpp
	AudioLoaders/WAVLoader.cpp
)

add_library(libaudio ${LIBAUDIO_SOURCES})
banan_link_library(libaudio ban)
banan_link_library(libaudio libc)

banan_install_headers(libaudio)
install(TARGETS libaudio OPTIONAL)
