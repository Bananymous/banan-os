set(USERSPACE_LIBRARIES
	LibC
	LibELF
	LibFont
	LibGUI
	LibImage
	LibInput
)

foreach(library ${USERSPACE_LIBRARIES})
	add_subdirectory(${library})
endforeach()

add_custom_target(libraries)

foreach(library ${USERSPACE_LIBRARIES})
	string(TOLOWER ${library} library_lower)
	if (TARGET ${library_lower})
		add_dependencies(libraries ${library_lower})
		# This is to allow cmake to link when libc updates
		target_link_options(${library_lower} PRIVATE -nolibc)
	endif()
endforeach()
