set(SOURCES
	main.cpp
)

add_executable(test-sort ${SOURCES})
banan_include_headers(test-sort ban)
banan_link_library(test-sort libc)

install(TARGETS test-sort OPTIONAL)
