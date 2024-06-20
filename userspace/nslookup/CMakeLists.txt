set(SOURCES
	main.cpp
)

add_executable(nslookup ${SOURCES})
banan_link_library(nslookup libc)

install(TARGETS nslookup OPTIONAL)
