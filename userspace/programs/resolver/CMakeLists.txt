set(SOURCES
	main.cpp
)

add_executable(resolver ${SOURCES})
banan_link_library(resolver ban)
banan_link_library(resolver libc)

install(TARGETS resolver OPTIONAL)
