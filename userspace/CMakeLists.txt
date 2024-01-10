cmake_minimum_required(VERSION 3.26)

project(userspace CXX)

set(USERSPACE_PROJECTS
	cat
	cat-mmap
	chmod
	cp
	dd
	echo
	id
	init
	image
	loadkeys
	ls
	meminfo
	mkdir
	mmap-shared-test
	poweroff
	rm
	Shell
	sleep
	snake
	stat
	sudo
	sync
	tee
	test
	test-framebuffer
	test-globals
	test-mouse
	test-sort
	touch
	u8sum
	whoami
	yes
)

foreach(USERSPACE_PROJECT ${USERSPACE_PROJECTS})
	add_subdirectory(${USERSPACE_PROJECT})
endforeach()

add_custom_target(userspace)
add_custom_target(userspace-install DEPENDS userspace)

add_subdirectory(aoc2023)

foreach(USERSPACE_PROJECT ${USERSPACE_PROJECTS})
	target_compile_options(${USERSPACE_PROJECT} PRIVATE -g)
	add_dependencies(${USERSPACE_PROJECT} libc-install ban-install)
	add_dependencies(userspace ${USERSPACE_PROJECT})
	add_dependencies(userspace-install ${USERSPACE_PROJECT}-install)
endforeach()
