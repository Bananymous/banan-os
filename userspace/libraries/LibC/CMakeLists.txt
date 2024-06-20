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

add_library(crt0 OBJECT arch/${BANAN_ARCH}/crt0.S)
add_library(crti OBJECT arch/${BANAN_ARCH}/crti.S)
add_library(crtn OBJECT arch/${BANAN_ARCH}/crtn.S)
add_custom_target(crtx-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_OBJECTS:crt0> ${CMAKE_INSTALL_LIBDIR}/crt0.o
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_OBJECTS:crti> ${CMAKE_INSTALL_LIBDIR}/crti.o
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_OBJECTS:crtn> ${CMAKE_INSTALL_LIBDIR}/crtn.o
	DEPENDS crt0 crti crtn sysroot
)

banan_include_headers(libc ban)
banan_include_headers(libc kernel)

banan_install_headers(libc)
install(TARGETS libc OPTIONAL)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
