set(SOURCES
	main.cpp
)

add_executable(tee ${SOURCES})
banan_link_library(tee libc)

install(TARGETS tee OPTIONAL)
