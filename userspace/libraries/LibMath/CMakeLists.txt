set(SOURCES
	dummy.cpp
)

add_library(libmath-static STATIC ${SOURCES})
add_library(libmath-shared SHARED ${SOURCES})

target_link_options(libmath-static PRIVATE -nolibc)
target_link_options(libmath-shared PRIVATE -nolibc)

banan_link_library(libmath-static libc)
banan_link_library(libmath-shared libc)

set_target_properties(libmath-static PROPERTIES OUTPUT_NAME libm)
set_target_properties(libmath-shared PROPERTIES OUTPUT_NAME libm)

install(TARGETS libmath-static OPTIONAL)
install(TARGETS libmath-shared OPTIONAL)
