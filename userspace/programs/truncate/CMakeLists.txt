set(SOURCES
	main.cpp
)

add_executable(truncate ${SOURCES})
banan_link_library(truncate libc)

install(TARGETS truncate OPTIONAL)
