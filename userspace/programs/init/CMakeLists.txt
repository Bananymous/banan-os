set(SOURCES
	main.cpp
)

add_executable(init ${SOURCES})
banan_include_headers(init ban)
banan_link_library(init libc-static)

target_link_options(init PRIVATE -static-libgcc)

install(TARGETS init OPTIONAL)
