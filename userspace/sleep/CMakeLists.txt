set(SOURCES
	main.cpp
)

add_executable(sleep ${SOURCES})
banan_link_library(sleep libc)

install(TARGETS sleep)
