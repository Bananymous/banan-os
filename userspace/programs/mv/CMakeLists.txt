set(SOURCES
	main.cpp
)

add_executable(mv ${SOURCES})
banan_link_library(mv libc)

install(TARGETS mv OPTIONAL)
