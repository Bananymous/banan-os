set(SOURCES
	main.cpp
)

add_executable(loadkeys ${SOURCES})
banan_link_library(loadkeys libc)

install(TARGETS loadkeys)
