set(SOURCES
	main.cpp
)

add_executable(uname ${SOURCES})
banan_link_library(uname libc)

install(TARGETS uname OPTIONAL)
