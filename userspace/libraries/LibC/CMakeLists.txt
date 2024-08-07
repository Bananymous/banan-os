set(LIBC_SOURCES
	arpa/inet.cpp
	assert.cpp
	ctype.cpp
	dirent.cpp
	fcntl.cpp
	grp.cpp
	inttypes.cpp
	locale.cpp
	malloc.cpp
	math.cpp
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
	sys/time.cpp
	sys/utsname.cpp
	sys/wait.cpp
	termios.cpp
	time.cpp
	unistd.cpp
	icxxabi.cpp

	../../../BAN/BAN/Assert.cpp
)

add_library(libc ${LIBC_SOURCES})
target_compile_definitions(libc PRIVATE __arch=${BANAN_ARCH})
target_compile_definitions(libc PRIVATE __enable_sse=${BANAN_ENABLE_SSE})
if (NOT BANAN_ENABLE_SSE)
	target_compile_options(libc PRIVATE -mno-sse -mno-sse2)
endif ()

target_compile_options(libc PRIVATE -O2 -g -Wstack-usage=512 -fno-tree-loop-distribute-patterns -fno-exceptions -nostdlib)
target_compile_options(libc PUBLIC -Wall -Wextra -Werror -Wno-error=stack-usage=)

function(add_crtx crtx)
	add_custom_target(${crtx}
		COMMAND ${CMAKE_CXX_COMPILER} -c -o ${CMAKE_INSTALL_LIBDIR}/${crtx}.o ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/${crtx}.S
		DEPENDS sysroot
	)
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
