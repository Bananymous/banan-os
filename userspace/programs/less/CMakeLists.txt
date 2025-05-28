set(SOURCES
	main.cpp
)

add_executable(less ${SOURCES})
banan_link_library(less ban)
banan_link_library(less libc)

install(TARGETS less OPTIONAL)
