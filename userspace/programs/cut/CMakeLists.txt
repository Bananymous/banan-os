set(SOURCES
	main.cpp
)

add_executable(cut ${SOURCES})
banan_link_library(cut libc)

install(TARGETS cut OPTIONAL)
