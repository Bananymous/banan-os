set(SOURCES
	main.cpp
)

add_executable(echo ${SOURCES})
banan_link_library(echo libc)

install(TARGETS echo OPTIONAL)
