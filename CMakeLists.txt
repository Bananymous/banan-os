cmake_minimum_required(VERSION 3.26)

if(DEFINED ENV{BANAN_ARCH})
	set(BANAN_ARCH $ENV{BANAN_ARCH})
else()
	set(BANAN_ARCH x86_64)
endif()

set(TOOLCHAIN_PREFIX ${CMAKE_SOURCE_DIR}/toolchain/local)

if(EXISTS ${TOOLCHAIN_PREFIX}/bin/${BANAN_ARCH}-banan_os-g++)
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED True)
	set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}/bin/${BANAN_ARCH}-banan_os-g++)
	set(CMAKE_CXX_COMPILER_WORKS True)

	set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}/bin/${BANAN_ARCH}-banan_os-gcc)
	set(CMAKE_C_COMPILER_WORKS True)
endif()

if(DEFINED QEMU_ACCEL)
	set(QEMU_ACCEL -accel ${QEMU_ACCEL})
endif()

if(DEFINED UEFI_BOOT)
	set(UEFI_BOOT 1)
endif()

add_compile_options(-mno-sse -mno-sse2)
add_compile_definitions(__enable_sse=0)

project(banan-os CXX)

set(BANAN_BASE_SYSROOT ${CMAKE_SOURCE_DIR}/base-sysroot.tar.gz)
set(BANAN_SYSROOT ${CMAKE_BINARY_DIR}/sysroot)
set(BANAN_INCLUDE ${BANAN_SYSROOT}/usr/include)
set(BANAN_LIB ${BANAN_SYSROOT}/usr/lib)
set(BANAN_BIN ${BANAN_SYSROOT}/usr/bin)
set(BANAN_BOOT ${BANAN_SYSROOT}/boot)
set(DISK_IMAGE_PATH ${CMAKE_BINARY_DIR}/banan-os.img)

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

add_custom_target(toolchain
	COMMAND ${CMAKE_COMMAND} -E env SYSROOT="${BANAN_SYSROOT}" PREFIX="${TOOLCHAIN_PREFIX}" ARCH="${BANAN_ARCH}" ${CMAKE_SOURCE_DIR}/toolchain/build.sh
	DEPENDS headers
	USES_TERMINAL
)

add_custom_target(libstdc++
	COMMAND ${CMAKE_COMMAND} -E env LIBSTDCPP="1" ${CMAKE_SOURCE_DIR}/toolchain/build.sh
	DEPENDS libc-install
	USES_TERMINAL
)

add_custom_target(image
	COMMAND ${CMAKE_COMMAND} -E env BANAN_ARCH="${BANAN_ARCH}" SYSROOT="${BANAN_SYSROOT}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" TOOLCHAIN="${TOOLCHAIN_PREFIX}" UEFI_BOOT="${UEFI_BOOT}" ${CMAKE_SOURCE_DIR}/image.sh
	DEPENDS kernel-install
	DEPENDS ban-install
	DEPENDS libc-install
	DEPENDS userspace-install
	DEPENDS libelf-install
	USES_TERMINAL
)

add_custom_target(image-full
	COMMAND ${CMAKE_COMMAND} -E env BANAN_ARCH="${BANAN_ARCH}" SYSROOT="${BANAN_SYSROOT}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" TOOLCHAIN="${TOOLCHAIN_PREFIX}" UEFI_BOOT="${UEFI_BOOT}" ${CMAKE_SOURCE_DIR}/image-full.sh
	DEPENDS kernel-install
	DEPENDS ban-install
	DEPENDS libc-install
	DEPENDS userspace-install
	DEPENDS libelf-install
	USES_TERMINAL
)

add_custom_target(check-fs
	COMMAND ${CMAKE_COMMAND} -E env DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" ${CMAKE_SOURCE_DIR}/check-fs.sh
	USES_TERMINAL
)

add_custom_target(qemu
	COMMAND ${CMAKE_COMMAND} -E env BANAN_ARCH="${BANAN_ARCH}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" UEFI_BOOT="${UEFI_BOOT}" ${CMAKE_SOURCE_DIR}/qemu.sh -serial stdio ${QEMU_ACCEL}
	DEPENDS image
	USES_TERMINAL
)

add_custom_target(qemu-nographic
	COMMAND ${CMAKE_COMMAND} -E env BANAN_ARCH="${BANAN_ARCH}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" UEFI_BOOT="${UEFI_BOOT}" ${CMAKE_SOURCE_DIR}/qemu.sh -nographic ${QEMU_ACCEL}
	DEPENDS image
	USES_TERMINAL
)

add_custom_target(qemu-debug
	COMMAND ${CMAKE_COMMAND} -E env BANAN_ARCH="${BANAN_ARCH}" DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" UEFI_BOOT="${UEFI_BOOT}" ${CMAKE_SOURCE_DIR}/qemu.sh -serial stdio -d int -no-reboot
	DEPENDS image
	USES_TERMINAL
)

add_custom_target(bochs
	COMMAND ${CMAKE_COMMAND} -E env DISK_IMAGE_PATH="${DISK_IMAGE_PATH}" ${CMAKE_SOURCE_DIR}/bochs.sh
	DEPENDS image
	USES_TERMINAL
)
