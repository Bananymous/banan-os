set(SOURCES
	main.cpp
)

add_executable(u8sum ${SOURCES})
banan_link_library(u8sum libc)

install(TARGETS u8sum)
