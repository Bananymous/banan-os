set(USERSPACE_TESTS
	test-fork
	test-framebuffer
	test-globals
	test-mmap-shared
	test-mouse
	test-popen
	test-setjmp
	test-shared
	test-sort
	test-tcp
	test-udp
	test-unix-socket
	test-window
)

foreach(project ${USERSPACE_TESTS})
	add_subdirectory(${project})
	add_dependencies(userspace ${project})
	# This is to allow cmake to link when libc updates
	target_link_options(${project} PRIVATE -nolibc)
	# Default compile options
	target_compile_options(${project} PRIVATE -g -O2)
endforeach()
