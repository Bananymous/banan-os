set(SOURCES
	main.cpp
)

add_executable(dd ${SOURCES})
banan_link_library(dd libc)

install(TARGETS dd)
