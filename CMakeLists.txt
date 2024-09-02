cmake_minimum_required(VERSION 3.26)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "banan-os")
	message(FATAL_ERROR "CMAKE_SYSTEM_NAME is not banan-os")
endif ()

set(BANAN_ENABLE_SSE 1)

project(banan-os CXX C ASM)

set(BANAN_BASE_SYSROOT ${CMAKE_SOURCE_DIR}/base-sysroot.tar.gz)

set(BANAN_INCLUDE ${BANAN_SYSROOT}/usr/include)
set(BANAN_LIB     ${BANAN_SYSROOT}/usr/lib)
set(BANAN_BIN     ${BANAN_SYSROOT}/usr/bin)
set(BANAN_ETC     ${BANAN_SYSROOT}/usr/etc)
set(BANAN_SHARE   ${BANAN_SYSROOT}/usr/share)
set(BANAN_BOOT    ${BANAN_SYSROOT}/boot)

set(CMAKE_INSTALL_BINDIR     ${BANAN_BIN})
set(CMAKE_INSTALL_SBINDIR    ${BANAN_BIN})
set(CMAKE_INSTALL_LIBDIR     ${BANAN_LIB})
set(CMAKE_INSTALL_INCLUDEDIR ${BANAN_INCLUDE})
set(CMAKE_INSTALL_SYSCONF    ${BANAN_ETC})
set(CMAKE_INSTALL_MESSAGE    NEVER)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY True)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# include headers of ${library} to ${target}
function(banan_include_headers target library)
	target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:${library},SOURCE_DIR>/include)
endfunction()

# include headers and link ${library} to ${target}
function(banan_link_library target library)
	target_link_libraries(${target} PRIVATE ${library})
	banan_include_headers(${target} ${library})
endfunction()

# add install step for all header files of target
function(banan_install_headers target)
	file(GLOB_RECURSE headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/include *.h)
	foreach(header ${headers})
		get_filename_component(subdirectory ${header} DIRECTORY)
		install(FILES include/${header} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${subdirectory})
	endforeach()
	target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endfunction()

add_subdirectory(kernel)
add_subdirectory(bootloader)
add_subdirectory(BAN)
add_subdirectory(userspace)

add_custom_target(sysroot
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BANAN_SYSROOT}
	COMMAND cd ${BANAN_SYSROOT} && tar xf ${BANAN_BASE_SYSROOT}
)
