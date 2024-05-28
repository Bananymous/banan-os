cmake_minimum_required(VERSION 3.26)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "banan-os")
	message(FATAL_ERROR "CMAKE_SYSTEM_NAME is not banan-os")
endif ()

#add_compile_options(-mno-sse -mno-sse2)
add_compile_definitions(__enable_sse=1)

project(banan-os CXX C ASM)

set(BANAN_BASE_SYSROOT ${CMAKE_SOURCE_DIR}/base-sysroot.tar.gz)
set(BANAN_INCLUDE ${BANAN_SYSROOT}/usr/include)
set(BANAN_LIB ${BANAN_SYSROOT}/usr/lib)
set(BANAN_BIN ${BANAN_SYSROOT}/usr/bin)
set(BANAN_SHARE ${BANAN_SYSROOT}/usr/share)
set(BANAN_BOOT ${BANAN_SYSROOT}/boot)

add_subdirectory(kernel)
add_subdirectory(bootloader)
add_subdirectory(BAN)
add_subdirectory(libc)
add_subdirectory(LibELF)
add_subdirectory(LibInput)
add_subdirectory(userspace)

add_custom_target(sysroot
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BANAN_SYSROOT}
	COMMAND cd ${BANAN_SYSROOT} && tar xf ${BANAN_BASE_SYSROOT}
)

add_custom_target(headers
	DEPENDS kernel-headers
	DEPENDS ban-headers
	DEPENDS libc-headers
	DEPENDS libelf-headers
	DEPENDS libinput-headers
)

add_custom_target(install-sysroot
	DEPENDS kernel-install
	DEPENDS ban-install
	DEPENDS libc-install
	DEPENDS userspace-install
	DEPENDS libelf-install
	DEPENDS libinput-install
)

add_custom_target(package-sysroot
	COMMAND cd ${BANAN_SYSROOT} && tar cf ${BANAN_SYSROOT_TAR} *
	DEPENDS install-sysroot
)
