set(SOURCES
	dummy.cpp
)

add_library(libelf ${SOURCES})

banan_install_headers(libelf)
install(TARGETS libelf)
