set(SOURCES
	main.cpp
)

add_executable(cat ${SOURCES})
banan_link_library(cat libc)

install(TARGETS cat)
