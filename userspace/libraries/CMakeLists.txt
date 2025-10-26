set(USERSPACE_LIBRARIES
	LibAudio
	LibC
	LibDEFLATE
	LibDL
	LibELF
	LibFont
	LibGUI
	LibImage
	LibInput
	LibMath
	LibPthread
)

foreach(library ${USERSPACE_LIBRARIES})
	add_subdirectory(${library})
endforeach()

add_custom_target(libraries)

foreach(library ${USERSPACE_LIBRARIES})
	string(TOLOWER ${library} library_lower)
	if (TARGET ${library_lower} AND NOT ${library_lower} STREQUAL "libc")
		add_dependencies(libraries ${library_lower})
		# This is to allow cmake to link when libc updates
		target_link_options(${library_lower} PRIVATE -nolibc)
		# Default compile options
		target_compile_options(${library_lower} PRIVATE -g -O2 -Wall -Wextra -Werror -msse -msse2)

		# set SONAME as cmake doesn't set it for some reason??
		set_target_properties(${library_lower} PROPERTIES LINK_FLAGS "-Wl,-soname,${library_lower}.so")
	endif()
endforeach()
