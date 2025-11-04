set(SOURCES
	main.cpp
)

add_executable(kill ${SOURCES})
banan_link_library(kill ban)
banan_link_library(kill libc)

install(TARGETS kill OPTIONAL)
