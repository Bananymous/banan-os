set(LIBC_SOURCES
	arpa/inet.cpp
	assert.cpp
	ctype.cpp
	dirent.cpp
	dlfcn.cpp
	errno.cpp
	fcntl.cpp
	ftw.cpp
	grp.cpp
	inttypes.cpp
	locale.cpp
	malloc.cpp
	math.cpp
	netdb.cpp
	poll.cpp
	printf_impl.cpp
	pwd.cpp
	scanf_impl.cpp
	setjmp.cpp
	signal.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	strings.cpp
	sys/banan-os.cpp
	sys/ioctl.cpp
	sys/mman.cpp
	sys/resource.cpp
	sys/select.cpp
	sys/socket.cpp
	sys/stat.cpp
	sys/time.cpp
	sys/utsname.cpp
	sys/wait.cpp
	syslog.cpp
	termios.cpp
	time.cpp
	unistd.cpp
	utime.cpp
	wchar.cpp
	icxxabi.cpp

	arch/${BANAN_ARCH}/setjmp.S

	../../../BAN/BAN/Assert.cpp
)

add_library(objlibc OBJECT ${LIBC_SOURCES})
target_compile_definitions(objlibc PRIVATE __arch=${BANAN_ARCH})
target_compile_definitions(objlibc PRIVATE __enable_sse=${BANAN_ENABLE_SSE})
if (NOT BANAN_ENABLE_SSE)
	target_compile_options(objlibc PRIVATE -mno-sse -mno-sse2)
endif ()

target_compile_options(objlibc PRIVATE -O2 -g -Wstack-usage=512 -fno-tree-loop-distribute-patterns -fno-exceptions -fpic -nolibc)
target_compile_options(objlibc PUBLIC -Wall -Wextra -Werror -Wno-error=stack-usage=)

function(add_crtx crtx)
	add_custom_target(${crtx}
		COMMAND ${CMAKE_CXX_COMPILER} -c -o ${CMAKE_INSTALL_LIBDIR}/${crtx}.o ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/${crtx}.S
		DEPENDS sysroot
	)
	add_dependencies(objlibc ${crtx})
endfunction()

add_crtx(crt0)
add_crtx(crti)
add_crtx(crtn)

banan_include_headers(objlibc ban)
banan_include_headers(objlibc kernel)

banan_install_headers(objlibc)

add_library(libc-static STATIC $<TARGET_OBJECTS:objlibc>)
add_library(libc-shared SHARED $<TARGET_OBJECTS:objlibc>)

target_link_options(libc-static PRIVATE -nostdlib)
target_link_options(libc-shared PRIVATE -nostdlib)

install(TARGETS libc-static OPTIONAL)
install(TARGETS libc-shared OPTIONAL)

set_target_properties(libc-static PROPERTIES OUTPUT_NAME libc)
set_target_properties(libc-shared PROPERTIES OUTPUT_NAME libc)

add_library(libc ALIAS libc-static)

execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc_s.so OUTPUT_VARIABLE LIBGCC_S_LINK OUTPUT_STRIP_TRAILING_WHITESPACE)
file(REAL_PATH ${LIBGCC_S_LINK} LIBGCC_S)
install(FILES ${LIBGCC_S} DESTINATION ${CMAKE_INSTALL_LIBDIR} RENAME libgcc_s.so
	PERMISSIONS
		OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ             GROUP_EXECUTE
		WORLD_READ             WORLD_EXECUTE
)
