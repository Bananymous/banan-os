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
	if (TARGET ${library_lower} AND NOT ${library_lower} STREQUAL "libc")
		add_dependencies(libraries ${library_lower})
		# This is to allow cmake to link when libc updates
		target_link_options(${library_lower} PRIVATE -nolibc)

		target_compile_definitions(${library_lower} PRIVATE __enable_sse=${BANAN_ENABLE_SSE})
		if (NOT BANAN_ENABLE_SSE)
			target_compile_options(${library_lower} PRIVATE -mno-sse -mno-sse2)
		endif ()
	endif()
endforeach()
