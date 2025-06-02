set(SOURCES
	dummy.cpp
)

add_library(libpthread-static STATIC ${SOURCES})
add_library(libpthread-shared SHARED ${SOURCES})

target_link_options(libpthread-static PRIVATE -nolibc)
target_link_options(libpthread-shared PRIVATE -nolibc)

banan_link_library(libpthread-static libc)
banan_link_library(libpthread-shared libc)

set_target_properties(libpthread-static PROPERTIES OUTPUT_NAME libpthread)
set_target_properties(libpthread-shared PROPERTIES OUTPUT_NAME libpthread)

install(TARGETS libpthread-static OPTIONAL)
install(TARGETS libpthread-shared OPTIONAL)
