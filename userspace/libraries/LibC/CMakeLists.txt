set(LIBC_SOURCES
	arpa/inet.cpp
	assert.cpp
	ctype.cpp
	dirent.cpp
	fcntl.cpp
	grp.cpp
	malloc.cpp
	netdb.cpp
	printf_impl.cpp
	pwd.cpp
	scanf_impl.cpp
	signal.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	strings.cpp
	stropts.cpp
	sys/banan-os.cpp
	sys/mman.cpp
	sys/select.cpp
	sys/socket.cpp
	sys/stat.cpp
	sys/wait.cpp
	termios.cpp
	time.cpp
	unistd.cpp
	math.cpp
	icxxabi.cpp

	../../../BAN/BAN/Assert.cpp
)

add_library(libc ${LIBC_SOURCES})
target_compile_options(libc PRIVATE -O2 -g -Wstack-usage=512 -fno-tree-loop-distribute-patterns -nostdlib)
target_compile_options(libc PUBLIC -Wall -Wextra -Werror -Wno-error=stack-usage=)

function(add_crtx crtx)
	add_custom_target(${crtx} COMMAND ${CMAKE_CXX_COMPILER} -c -o ${crtx}.o ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/${crtx}.S)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${crtx}.o DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
	add_dependencies(libc ${crtx})
endfunction()

add_crtx(crt0)
add_crtx(crti)
add_crtx(crtn)

banan_include_headers(libc ban)
banan_include_headers(libc kernel)

banan_install_headers(libc)
install(TARGETS libc OPTIONAL)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
