set(TEST_SOURCES
	test.cpp
)

add_executable(test ${TEST_SOURCES})
banan_link_library(test libc)

install(TARGETS test)
