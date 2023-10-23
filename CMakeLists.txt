cmake_minimum_required(VERSION 3.26)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "banan-os")
	message(FATAL_ERROR "CMAKE_SYSTEM_NAME is not banan-os")
endif ()

add_compile_options(-mno-sse -mno-sse2)
add_compile_definitions(__enable_sse=0)

project(banan-os CXX C ASM)

set(BANAN_BASE_SYSROOT ${CMAKE_SOURCE_DIR}/base-sysroot.tar.gz)
set(BANAN_SYSROOT ${CMAKE_BINARY_DIR}/sysroot)
set(BANAN_INCLUDE ${BANAN_SYSROOT}/usr/include)
set(BANAN_LIB ${BANAN_SYSROOT}/usr/lib)
set(BANAN_BIN ${BANAN_SYSROOT}/usr/bin)
set(BANAN_BOOT ${BANAN_SYSROOT}/boot)

add_subdirectory(kernel)
add_subdirectory(BAN)
add_subdirectory(libc)
add_subdirectory(LibELF)
add_subdirectory(userspace)

add_custom_target(sysroot
	COMMAND mkdir -p ${BANAN_SYSROOT}
	COMMAND cd ${BANAN_SYSROOT} && sudo tar xf ${BANAN_BASE_SYSROOT}
	USES_TERMINAL
)

add_custom_target(headers
	DEPENDS kernel-headers
	DEPENDS ban-headers
	DEPENDS libc-headers
	DEPENDS libelf-headers
)

add_custom_target(install-sysroot
	DEPENDS kernel-install
	DEPENDS ban-install
	DEPENDS libc-install
	DEPENDS userspace-install
	DEPENDS libelf-install
)
