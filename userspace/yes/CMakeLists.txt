set(SOURCES
	main.cpp
)

add_executable(yes ${SOURCES})
banan_link_library(yes libc)

install(TARGETS yes)
