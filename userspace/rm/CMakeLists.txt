set(SOURCES
	main.cpp
)

add_executable(rm ${SOURCES})
banan_include_headers(rm ban)
banan_link_library(rm libc)

install(TARGETS rm OPTIONAL)
