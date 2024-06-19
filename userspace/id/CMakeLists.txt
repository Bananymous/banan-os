set(SOURCES
	main.cpp
)

add_executable(id ${SOURCES})
banan_include_headers(id ban)
banan_link_library(id libc)

install(TARGETS id)
