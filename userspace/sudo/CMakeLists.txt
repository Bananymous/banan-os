set(SOURCES
	main.cpp
)

add_executable(sudo ${SOURCES})
banan_link_library(sudo libc)

install(TARGETS sudo OPTIONAL)
