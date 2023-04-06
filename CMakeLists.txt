cmake_minimum_required(VERSION 3.26)

if(DEFINED ENV{BANAN_ARCH})
	set(BANAN_ARCH $ENV{BANAN_ARCH})
else()
	set(BANAN_ARCH x86_64)
endif()

set(TOOLCHAIN_PREFIX ${CMAKE_SOURCE_DIR}/toolchain/local)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}/bin/${BANAN_ARCH}-banan_os-g++)
set(CMAKE_CXX_COMPILER_WORKS True)

if(NOT EXISTS ${CMAKE_CXX_COMPILER})
	set(CMAKE_CXX_COMPILER g++)
endif()

project(banan-os CXX)

set(BANAN_SYSROOT ${CMAKE_BINARY_DIR}/sysroot)
set(BANAN_INCLUDE ${BANAN_SYSROOT}/usr/include)
set(BANAN_LIB ${BANAN_SYSROOT}/usr/lib)
set(BANAN_BOOT ${BANAN_SYSROOT}/boot)
set(DISK_IMAGE_PATH ${CMAKE_BINARY_DIR}/banan-os.img)

add_subdirectory(kernel)
add_subdirectory(BAN)
add_subdirectory(libc)

add_custom_target(sysroot
	COMMAND mkdir -p ${BANAN_SYSROOT}
	COMMAND mkdir -p ${BANAN_INCLUDE}
	COMMAND mkdir -p ${BANAN_LIB}
	COMMAND mkdir -p ${BANAN_BOOT}
	COMMAND cp -r ${CMAKE_SOURCE_DIR}/base/* ${BANAN_SYSROOT}/
)

add_custom_target(headers
	DEPENDS kernel-headers
	DEPENDS ban-headers
	DEPENDS libc-headers
)

add_custom_target(toolchain
	COMMAND ${CMAKE_COMMAND} -E env SYSROOT="${BANAN_SYSROOT}" PREFIX="${TOOLCHAIN_PREFIX}" ARCH="${BANAN_ARCH}" ${CMAKE_SOURCE_DIR}/toolchain/build.sh
	DEPENDS headers
	USES_TERMINAL
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
