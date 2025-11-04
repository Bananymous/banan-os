set(SOURCES
	main.cpp
)

add_executable(pwd ${SOURCES})
banan_link_library(pwd libc)

install(TARGETS pwd OPTIONAL)
