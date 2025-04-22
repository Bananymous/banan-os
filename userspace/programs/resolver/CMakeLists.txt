set(SOURCES
	main.cpp
)

add_executable(resolver ${SOURCES})
banan_link_library(resolver ban)
banan_link_library(resolver libc)

target_compile_options(resolver PRIVATE -Wno-maybe-uninitialized)

install(TARGETS resolver OPTIONAL)
