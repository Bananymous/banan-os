set(SOURCES
	main.cpp
)

add_executable(env ${SOURCES})
banan_link_library(env ban)
banan_link_library(env libc)

install(TARGETS env OPTIONAL)
