set(SOURCES
	main.cpp
)

add_executable(test-setjmp ${SOURCES})
banan_link_library(test-setjmp libc)

install(TARGETS test-setjmp OPTIONAL)
