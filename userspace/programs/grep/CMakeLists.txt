set(SOURCES
	main.cpp
)

add_executable(grep ${SOURCES})
banan_link_library(grep libc)

install(TARGETS grep OPTIONAL)
