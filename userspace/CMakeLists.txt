set(USERSPACE_PROJECTS
	cat
	cat-mmap
	chmod
	cp
	dd
	dhcp-client
	echo
	getopt
	http-server
	id
	image
	init
	loadkeys
	ls
	meminfo
	mkdir
	nslookup
	poweroff
	resolver
	rm
	Shell
	sleep
	snake
	stat
	sudo
	sync
	tee
	Terminal
	test
	test-framebuffer
	test-globals
	test-mmap-shared
	test-mouse
	test-popen
	test-sort
	test-tcp
	test-udp
	test-unix-socket
	test-window
	touch
	u8sum
	whoami
	WindowServer
	yes
)

add_subdirectory(libraries)

add_custom_target(userspace)

#add_subdirectory(aoc2023)

foreach(project ${USERSPACE_PROJECTS})
	add_subdirectory(${project})
	add_dependencies(userspace ${project})
	# This is to allow cmake to link when libc updates
	target_link_options(${project} PRIVATE -nolibc)
	# Default compile options
	target_compile_options(${project} PRIVATE -g -O2)
endforeach()
