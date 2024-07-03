set(SOURCES
	main.cpp
)

add_executable(poweroff ${SOURCES})
banan_link_library(poweroff libc)

install(TARGETS poweroff OPTIONAL)
