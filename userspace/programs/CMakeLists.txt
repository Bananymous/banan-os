set(USERSPACE_PROGRAMS
	audio
	AudioServer
	bananfetch
	basename
	cat
	cat-mmap
	chmod
	chown
	ClipboardServer
	cp
	dd
	dhcp-client
	dirname
	DynamicLoader
	echo
	env
	false
	getopt
	http-server
	id
	image
	ImageViewer
	init
	kill
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
	pwd
	resolver
	rm
	Shell
	sleep
	snake
	stat
	sudo
	sync
	tee
	test
	true
	TaskBar
	Terminal
	touch
	u8sum
	uname
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
	target_compile_options(${project} PRIVATE -g -O2 -Wall -Wextra -Werror -msse -msse2)
endforeach()
