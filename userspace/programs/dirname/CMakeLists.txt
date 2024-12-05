set(SOURCES
	main.cpp
)

add_executable(dirname ${SOURCES})
banan_link_library(dirname libc)

install(TARGETS dirname OPTIONAL)
