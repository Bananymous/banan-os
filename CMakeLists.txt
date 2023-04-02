cmake_minimum_required(VERSION 3.26)

set(BANAN_ARCH x86_64)

if(DEFINED ENV{BANAN_ARCH})
	set(BANAN_ARCH $ENV{BANAN_ARCH})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER ${BANAN_ARCH}-elf-g++)
set(CMAKE_CXX_COMPILER_WORKS True)

if("${BANAN_ARCH}" STREQUAL "i386")
	set(CMAKE_CXX_COMPILER i686-elf-g++)
endif()

project(banan-os CXX)

set(BANAN_SYSROOT ${CMAKE_BINARY_DIR}/sysroot)
set(BANAN_INCLUDE ${BANAN_SYSROOT}/usr/include)
set(BANAN_BOOT ${BANAN_SYSROOT}/boot)
set(DISK_IMAGE_PATH ${CMAKE_BINARY_DIR}/banan-os.img)

add_subdirectory(kernel)
add_subdirectory(BAN)
add_subdirectory(libc)

add_custom_target(sysroot
	COMMAND mkdir -p ${BANAN_SYSROOT}
	COMMAND mkdir -p ${BANAN_BOOT}
	COMMAND mkdir -p ${BANAN_INCLUDE}
	COMMAND cp -r ${CMAKE_SOURCE_DIR}/base/* ${BANAN_SYSROOT}/
)

add_custom_target(image
	COMMAND ${CMAKE_COMMAND} -E env SYSROOT="${BANAN_SYSROOT}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" ${CMAKE_SOURCE_DIR}/image.sh
	DEPENDS kernel-install
	DEPENDS ban-install
	DEPENDS libc-install
	USES_TERMINAL
)

add_custom_target(image-full
	COMMAND ${CMAKE_COMMAND} -E env SYSROOT="${BANAN_SYSROOT}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" ${CMAKE_SOURCE_DIR}/image-full.sh
	DEPENDS kernel-install
	DEPENDS ban-install
	DEPENDS libc-install
	USES_TERMINAL
)

add_custom_target(qemu
	COMMAND ${CMAKE_COMMAND} -E env BANAN_ARCH="${BANAN_ARCH}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" ${CMAKE_SOURCE_DIR}/qemu.sh
	DEPENDS image
	USES_TERMINAL
)

add_custom_target(bochs
	COMMAND ${CMAKE_COMMAND} -E env DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" ${CMAKE_SOURCE_DIR}/bochs.sh
	DEPENDS image
	USES_TERMINAL
)
