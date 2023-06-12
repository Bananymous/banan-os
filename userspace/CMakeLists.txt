cmake_minimum_required(VERSION 3.26)

project(userspace CXX)

set(USERSPACE_PROJECTS
	cat
	echo
	id
	init
	ls
	Shell
	test
	u8sum
	yes
)

foreach(USERSPACE_PROJECT ${USERSPACE_PROJECTS})
	add_subdirectory(${USERSPACE_PROJECT})
endforeach()

add_custom_target(userspace)
add_custom_target(userspace-install DEPENDS userspace)

foreach(USERSPACE_PROJECT ${USERSPACE_PROJECTS})
	target_compile_options(${USERSPACE_PROJECT} PUBLIC -mno-sse -mno-sse2)
	add_dependencies(userspace ${USERSPACE_PROJECT})
	add_dependencies(userspace-install ${USERSPACE_PROJECT}-install)
endforeach()
