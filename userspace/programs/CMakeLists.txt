set(USERSPACE_PROGRAMS
	bananfetch
	basename
	cat
	cat-mmap
	chmod
	cp
	dd
	dhcp-client
	dirname
	DynamicLoader
	echo
	env
	getopt
	http-server
	id
	image
	init
	less
	ln
	loadfont
	loadkeys
	ls
	meminfo
	mkdir
	nologin
	nslookup
	poweroff
	ProgramLauncher
	resolver
	rm
	Shell
	sleep
	snake
	stat
	sudo
	sync
	tee
	TaskBar
	Terminal
	touch
	u8sum
	whoami
	WindowServer
	yes
)

foreach(project ${USERSPACE_PROGRAMS})
	add_subdirectory(${project})
	add_dependencies(userspace ${project})
	# This is to allow cmake to link when libc updates
	target_link_options(${project} PRIVATE -nolibc)
	# Default compile options
	target_compile_options(${project} PRIVATE -g -O2 -Wall -Wextra -Werror)
endforeach()
