set(SOURCES
	main.cpp
	HTTPServer.cpp
)

add_executable(http-server ${SOURCES})
banan_link_library(http-server ban)
banan_link_library(http-server libc)

install(TARGETS http-server OPTIONAL)
