cmake_minimum_required(VERSION 3.26)

project(libraries CXX)

set(USERSPACE_LIBRARIES
	LibC
	LibELF
	LibFont
	LibGUI
	LibImage
	LibInput
)

foreach(LIBRARY ${USERSPACE_LIBRARIES})
	add_subdirectory(${LIBRARY})
endforeach()

add_custom_target(libraries)
add_custom_target(libraries-headers)
add_custom_target(libraries-install)

foreach(LIBRARY ${USERSPACE_LIBRARIES})
	string(TOLOWER ${LIBRARY} LIBRARY_LOWER)

	if (TARGET ${LIBRARY_LOWER})
		add_dependencies(libraries ${LIBRARY_LOWER})
		# This is to allow cmake to link when libc updates
		target_link_options(${LIBRARY_LOWER} PRIVATE -nolibc)
	endif()

	add_dependencies(libraries-headers ${LIBRARY_LOWER}-headers)
	add_dependencies(libraries-install ${LIBRARY_LOWER}-install)
endforeach()
