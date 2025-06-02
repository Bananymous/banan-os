set(SOURCES
	main.cpp
)

add_executable(nologin ${SOURCES})
banan_link_library(nologin ban)
banan_link_library(nologin libc)

install(TARGETS nologin OPTIONAL)
