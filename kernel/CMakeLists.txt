cmake_minimum_required(VERSION 3.26)

project(kernel CXX C ASM)

if("${BANAN_ARCH}" STREQUAL "x86_64")
	set(ELF_FORMAT elf64-x86-64)
elseif("${BANAN_ARCH}" STREQUAL "i386")
	set(ELF_FORMAT elf32-i386)
endif()

set(KERNEL_SOURCES
	font/prefs.psf.o
	kernel/ACPI.cpp
	kernel/APIC.cpp
	kernel/CPUID.cpp
	kernel/Debug.cpp
	kernel/Device/Device.cpp
	kernel/Device/NullDevice.cpp
	kernel/Device/ZeroDevice.cpp
	kernel/Errors.cpp
	kernel/Font.cpp
	kernel/FS/DevFS/FileSystem.cpp
	kernel/FS/Ext2/FileSystem.cpp
	kernel/FS/Ext2/Inode.cpp
	kernel/FS/Inode.cpp
	kernel/FS/Pipe.cpp
	kernel/FS/RamFS/FileSystem.cpp
	kernel/FS/RamFS/Inode.cpp
	kernel/FS/VirtualFileSystem.cpp
	kernel/Input/PS2Controller.cpp
	kernel/Input/PS2Keyboard.cpp
	kernel/Input/PS2Keymap.cpp
	kernel/InterruptController.cpp
	kernel/kernel.cpp
	kernel/Memory/GeneralAllocator.cpp
	kernel/Memory/Heap.cpp
	kernel/Memory/kmalloc.cpp
	kernel/Memory/PhysicalRange.cpp
	kernel/Memory/VirtualRange.cpp
	kernel/Networking/E1000.cpp
	kernel/OpenFileDescriptorSet.cpp
	kernel/Panic.cpp
	kernel/PCI.cpp
	kernel/PIC.cpp
	kernel/Process.cpp
	kernel/Scheduler.cpp
	kernel/Semaphore.cpp
	kernel/SpinLock.cpp
	kernel/SSP.cpp
	kernel/Storage/ATABus.cpp
	kernel/Storage/ATAController.cpp
	kernel/Storage/ATADevice.cpp
	kernel/Storage/DiskCache.cpp
	kernel/Storage/StorageDevice.cpp
	kernel/Syscall.cpp
	kernel/Syscall.S
	kernel/Terminal/Serial.cpp
	kernel/Terminal/TTY.cpp
	kernel/Terminal/VesaTerminalDriver.cpp
	kernel/Terminal/VirtualTTY.cpp
	kernel/Thread.cpp
	kernel/Timer/HPET.cpp
	kernel/Timer/PIT.cpp
	kernel/Timer/RTC.cpp
	kernel/Timer/Timer.cpp
	icxxabi.cpp
)

#set(ENABLE_KERNEL_UBSAN True)

if(ENABLE_KERNEL_UBSAN)
	set(KERNEL_SOURCES ${KERNEL_SOURCES} ubsan.cpp)
endif()

if("${BANAN_ARCH}" STREQUAL "x86_64")
	set(KERNEL_SOURCES
		${KERNEL_SOURCES}
		arch/x86_64/boot.S
		arch/x86_64/GDT.cpp
		arch/x86_64/IDT.cpp
		arch/x86_64/interrupts.S
		arch/x86_64/PageTable.cpp
		arch/x86_64/Signal.S
		arch/x86_64/Thread.S
	)
elseif("${BANAN_ARCH}" STREQUAL "i386")
	set(KERNEL_SOURCES
		${KERNEL_SOURCES}
		arch/i386/boot.S
		arch/i386/GDT.cpp
		arch/i386/IDT.cpp
		arch/i386/MMU.cpp
		arch/i386/SpinLock.S
		arch/i386/Thread.S
	)
else()
	message(FATAL_ERROR "unsupported architecure ${BANAN_ARCH}")
endif()

file(GLOB_RECURSE LAI_SOURCES
	lai/*.c
)
set(LAI_SOURCES
	${LAI_SOURCES}
	kernel/lai_host.cpp
)

set(BAN_SOURCES
	../BAN/BAN/New.cpp
	../BAN/BAN/String.cpp
	../BAN/BAN/StringView.cpp
	../BAN/BAN/Time.cpp
)

set(LIBC_SOURCES
	../libc/ctype.cpp
	../libc/string.cpp
)

set(LIBELF_SOURCES
	../LibELF/LibELF/LoadableELF.cpp
)

set(KERNEL_SOURCES
	${KERNEL_SOURCES}
	${LAI_SOURCES}
	${BAN_SOURCES}
	${LIBC_SOURCES}
	${LIBELF_SOURCES}
)

add_executable(kernel ${KERNEL_SOURCES})
add_dependencies(kernel headers)

target_compile_definitions(kernel PUBLIC __is_kernel)
target_compile_definitions(kernel PUBLIC __arch=${BANAN_ARCH})

target_compile_options(kernel PUBLIC -O2 -g)
target_compile_options(kernel PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix -fno-rtti -fno-exceptions>)
target_compile_options(kernel PUBLIC -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)
target_compile_options(kernel PUBLIC -fstack-protector -ffreestanding -Wall -Werror=return-type -Wstack-usage=1024 -fno-omit-frame-pointer -mgeneral-regs-only)

if(ENABLE_KERNEL_UBSAN)
	target_compile_options(kernel PUBLIC -fsanitize=undefined)
endif()

if("${BANAN_ARCH}" STREQUAL "x86_64")
	target_compile_options(kernel PUBLIC -mcmodel=kernel -mno-red-zone -mno-mmx)
	target_link_options(kernel PUBLIC LINKER:-z,max-page-size=4096)
	target_link_options(kernel PUBLIC LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/linker.ld)
elseif("${BANAN_ARCH}" STREQUAL "i386")
	target_link_options(kernel PUBLIC LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/i386/linker.ld)
endif()

target_link_options(kernel PUBLIC -ffreestanding -nostdlib)

add_custom_target(crt0
	COMMAND ${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crt0.S -o ${CMAKE_CURRENT_BINARY_DIR}/crt0.o
	DEPENDS headers
)

add_custom_command(
	TARGET crt0
	POST_BUILD
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/crt0.o ${BANAN_LIB}/
)

add_custom_target(kernel-headers
	COMMAND sudo rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	COMMAND sudo rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/lai/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
	USES_TERMINAL
)

add_custom_target(kernel-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/kernel ${BANAN_BOOT}/banan-os.kernel
	DEPENDS kernel
	USES_TERMINAL
)

execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=crtbegin.o OUTPUT_VARIABLE CRTBEGIN OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=crtend.o OUTPUT_VARIABLE CRTEND OUTPUT_STRIP_TRAILING_WHITESPACE)

add_custom_command(
	TARGET kernel PRE_LINK
	COMMAND ${CMAKE_CXX_COMPILER} -MD -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crti.S ${COMPILE_OPTIONS}
	COMMAND ${CMAKE_CXX_COMPILER} -MD -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crtn.S ${COMPILE_OPTIONS}
	COMMAND cp ${CRTBEGIN} .
	COMMAND cp ${CRTEND} .
)

add_custom_command(
	TARGET kernel POST_BUILD
	COMMAND x86_64-banan_os-strip ${CMAKE_CURRENT_BINARY_DIR}/kernel
)

add_custom_command(
	OUTPUT font/prefs.psf.o
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && objcopy -O ${ELF_FORMAT} -B i386 -I binary font/prefs.psf ${CMAKE_CURRENT_BINARY_DIR}/font/prefs.psf.o
)

set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_COMPILER} <CMAKE_CXX_LINK_FLAGS> <FLAGS> <LINK_FLAGS> -o <TARGET> ${CMAKE_CURRENT_BINARY_DIR}/crti.o ${CMAKE_CURRENT_BINARY_DIR}/crtbegin.o <OBJECTS> ${CMAKE_CURRENT_BINARY_DIR}/crtend.o ${CMAKE_CURRENT_BINARY_DIR}/crtn.o -lgcc ")
