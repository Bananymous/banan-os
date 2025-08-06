set(SOURCES
	dummy.cpp
)

add_library(libdl-static STATIC ${SOURCES})
add_library(libdl-shared SHARED ${SOURCES})

target_link_options(libdl-static PRIVATE -nolibc)
target_link_options(libdl-shared PRIVATE -nolibc)

banan_link_library(libdl-static libc)
banan_link_library(libdl-shared libc)

set_target_properties(libdl-static PROPERTIES OUTPUT_NAME libdl)
set_target_properties(libdl-shared PROPERTIES OUTPUT_NAME libdl)

install(TARGETS libdl-static OPTIONAL)
install(TARGETS libdl-shared OPTIONAL)
