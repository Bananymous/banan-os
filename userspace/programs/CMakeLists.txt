set(USERSPACE_PROGRAMS
	bananfetch
	cat
	cat-mmap
	chmod
	cp
	dd
	dhcp-client
	DynamicLoader
	echo
	getopt
	http-server
	id
	image
	init
	loadfont
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
	target_compile_options(${project} PRIVATE -g -O2)

	target_compile_definitions(${project} PRIVATE __enable_sse=${BANAN_ENABLE_SSE})
	if (NOT BANAN_ENABLE_SSE)
		target_compile_options(${project} PRIVATE -mno-sse -mno-sse2)
	endif ()
endforeach()
