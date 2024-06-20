set(SOURCES
	main.cpp
)

add_executable(whoami ${SOURCES})
banan_include_headers(whoami ban)
banan_link_library(whoami libc)

install(TARGETS whoami OPTIONAL)
