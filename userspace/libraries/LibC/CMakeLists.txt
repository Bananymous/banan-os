set(LIBC_SOURCES
	arpa/inet.cpp
	assert.cpp
	complex.cpp
	ctype.cpp
	dirent.cpp
	dynamic-loader.cpp
	endian.cpp
	environ.cpp
	fcntl.cpp
	fenv.cpp
	fnmatch.cpp
	ftw.cpp
	getopt.cpp
	glob.cpp
	grp.cpp
	ifaddrs.cpp
	inttypes.cpp
	langinfo.cpp
	libgen.cpp
	libintl.cpp
	locale.cpp
	malloc.cpp
	math.cpp
	net/if.cpp
	netdb.cpp
	netinet/in.cpp
	poll.cpp
	printf_impl.cpp
	pthread.cpp
	pwd.cpp
	regex.cpp
	scanf_impl.cpp
	sched.cpp
	search.cpp
	semaphore.cpp
	setjmp.cpp
	signal.cpp
	spawn.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	strings.cpp
	sys/auxv.cpp
	sys/banan-os.cpp
	sys/epoll.cpp
	sys/eventfd.cpp
	sys/file.cpp
	sys/futex.cpp
	sys/ioctl.cpp
	sys/mman.cpp
	sys/random.cpp
	sys/resource.cpp
	sys/select.cpp
	sys/shm.cpp
	sys/socket.cpp
	sys/stat.cpp
	sys/statvfs.cpp
	sys/time.cpp
	sys/times.cpp
	sys/uio.cpp
	sys/utsname.cpp
	sys/wait.cpp
	syslog.cpp
	termios.cpp
	time.cpp
	unistd.cpp
	utime.cpp
	wchar-stdio.cpp
	wchar.cpp
	icxxabi.cpp

	arch/${BANAN_ARCH}/setjmp.S

	../../../BAN/BAN/Assert.cpp
)

add_library(objlibc OBJECT ${LIBC_SOURCES})

target_compile_definitions(objlibc PRIVATE __arch=${BANAN_ARCH} __is_libc)

target_compile_options(objlibc PRIVATE -O2 -g -Wstack-usage=8192 -fno-exceptions -fno-rtti -fpic -msse -msse2)
target_compile_options(objlibc PUBLIC -Wall -Wextra -Werror -Wno-error=stack-usage=)

banan_include_headers(objlibc ban)
banan_include_headers(objlibc kernel)
banan_include_headers(objlibc libelf)

banan_install_headers(objlibc)

add_library(libc-static STATIC $<TARGET_OBJECTS:objlibc>)
add_library(libc-shared SHARED $<TARGET_OBJECTS:objlibc>)

target_link_options(libc-static PRIVATE -nolibc -nostdlib++)
target_link_options(libc-shared PRIVATE -nolibc -nostdlib++)

set_target_properties(libc-static PROPERTIES OUTPUT_NAME libc)
set_target_properties(libc-shared PROPERTIES OUTPUT_NAME libc)

target_link_options(libc-shared PRIVATE LINKER:-e,_libc_entry LINKER:-z,text -static-libgcc)

add_library(libc ALIAS libc-shared)

install(TARGETS libc-static OPTIONAL)
install(TARGETS libc-shared OPTIONAL)

function(add_crtx crtx)
	set(crt_out ${CMAKE_INSTALL_LIBDIR}/${crtx}.o)
	set(crt_src ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/${crtx}.S)
	add_custom_command(
		OUTPUT ${crt_out}
		COMMAND ${CMAKE_C_COMPILER} -c -o ${crt_out} ${crt_src}
		DEPENDS ${crt_src}
	)
	add_custom_target(${crtx} DEPENDS ${crt_out})
	add_dependencies(objlibc ${crtx})
endfunction()

add_crtx(crt0)
add_crtx(crti)
add_crtx(crtn)

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.a\"  \"${CMAKE_INSTALL_LIBDIR}/libdl.a\")")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.so\" \"${CMAKE_INSTALL_LIBDIR}/libdl.so\")")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.a\"  \"${CMAKE_INSTALL_LIBDIR}/libm.a\")")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.so\" \"${CMAKE_INSTALL_LIBDIR}/libm.so\")")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.a\"  \"${CMAKE_INSTALL_LIBDIR}/libpthread.a\")")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.so\" \"${CMAKE_INSTALL_LIBDIR}/libpthread.so\")")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libc.so\" \"${CMAKE_INSTALL_LIBDIR}/DynamicLoader.so\")")

execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc_s.so OUTPUT_VARIABLE LIBGCC_S_LINK OUTPUT_STRIP_TRAILING_WHITESPACE)
file(REAL_PATH ${LIBGCC_S_LINK} LIBGCC_S)
install(FILES ${LIBGCC_S} DESTINATION ${CMAKE_INSTALL_LIBDIR} RENAME libgcc_s.so
	PERMISSIONS
		OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ             GROUP_EXECUTE
		WORLD_READ             WORLD_EXECUTE
)
