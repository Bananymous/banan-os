cmake_minimum_required(VERSION 3.26)

project(userspace CXX)

set(USERSPACE_PROJECTS
	cat
	cat-mmap
	chmod
	cp
	dd
	dhcp-client
	echo
	getopt
	id
	image
	init
	loadkeys
	ls
	meminfo
	mkdir
	mmap-shared-test
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

foreach(USERSPACE_PROJECT ${USERSPACE_PROJECTS})
	add_subdirectory(${USERSPACE_PROJECT})
endforeach()

add_custom_target(userspace DEPENDS libraries)
add_custom_target(userspace-headers DEPENDS libraries-headers)
add_custom_target(userspace-install DEPENDS userspace libraries-install)

add_subdirectory(aoc2023)

foreach(USERSPACE_PROJECT ${USERSPACE_PROJECTS})
	target_compile_options(${USERSPACE_PROJECT} PRIVATE -g)
	add_dependencies(userspace ${USERSPACE_PROJECT})
	add_dependencies(userspace-install ${USERSPACE_PROJECT}-install)

	target_link_options(${USERSPACE_PROJECT} PRIVATE -nolibc)
endforeach()
