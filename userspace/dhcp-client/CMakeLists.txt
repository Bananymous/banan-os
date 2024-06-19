set(SOURCES
	main.cpp
)

add_executable(dhcp-client ${SOURCES})
banan_include_headers(dhcp-client ban)
banan_link_library(dhcp-client libc)

install(TARGETS dhcp-client)
