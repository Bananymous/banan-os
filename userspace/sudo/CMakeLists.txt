set(SOURCES
	main.cpp
)

add_executable(sudo ${SOURCES})
banan_link_library(sudo libc)

install(
	TARGETS sudo
	PERMISSIONS
		OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE
		SETUID
	OPTIONAL
)
