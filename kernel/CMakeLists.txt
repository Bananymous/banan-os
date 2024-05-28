cmake_minimum_required(VERSION 3.26)

project(kernel CXX C ASM)

if("${BANAN_ARCH}" STREQUAL "x86_64")
	set(ELF_FORMAT elf64-x86-64)
elseif("${BANAN_ARCH}" STREQUAL "i686")
	set(ELF_FORMAT elf32-i386)
endif()

set(KERNEL_SOURCES
	font/prefs.psf.o
	kernel/ACPI/ACPI.cpp
	kernel/ACPI/AML.cpp
	kernel/ACPI/AML/Field.cpp
	kernel/ACPI/AML/NamedObject.cpp
	kernel/ACPI/AML/Namespace.cpp
	kernel/ACPI/AML/Node.cpp
	kernel/ACPI/AML/Package.cpp
	kernel/ACPI/AML/Scope.cpp
	kernel/APIC.cpp
	kernel/BootInfo.cpp
	kernel/CPUID.cpp
	kernel/Credentials.cpp
	kernel/Debug.cpp
	kernel/Device/DebugDevice.cpp
	kernel/Device/Device.cpp
	kernel/Device/FramebufferDevice.cpp
	kernel/Device/NullDevice.cpp
	kernel/Device/ZeroDevice.cpp
	kernel/Errors.cpp
	kernel/Font.cpp
	kernel/FS/DevFS/FileSystem.cpp
	kernel/FS/Ext2/FileSystem.cpp
	kernel/FS/Ext2/Inode.cpp
	kernel/FS/Inode.cpp
	kernel/FS/Pipe.cpp
	kernel/FS/ProcFS/FileSystem.cpp
	kernel/FS/ProcFS/Inode.cpp
	kernel/FS/TmpFS/FileSystem.cpp
	kernel/FS/TmpFS/Inode.cpp
	kernel/FS/VirtualFileSystem.cpp
	kernel/GDT.cpp
	kernel/IDT.cpp
	kernel/Input/PS2/Controller.cpp
	kernel/Input/PS2/Device.cpp
	kernel/Input/PS2/Keyboard.cpp
	kernel/Input/PS2/Keymap.cpp
	kernel/Input/PS2/Mouse.cpp
	kernel/Interruptable.cpp
	kernel/InterruptController.cpp
	kernel/kernel.cpp
	kernel/Memory/DMARegion.cpp
	kernel/Memory/FileBackedRegion.cpp
	kernel/Memory/Heap.cpp
	kernel/Memory/kmalloc.cpp
	kernel/Memory/MemoryBackedRegion.cpp
	kernel/Memory/MemoryRegion.cpp
	kernel/Memory/PhysicalRange.cpp
	kernel/Memory/VirtualRange.cpp
	kernel/Networking/ARPTable.cpp
	kernel/Networking/E1000/E1000.cpp
	kernel/Networking/E1000/E1000E.cpp
	kernel/Networking/IPv4Layer.cpp
	kernel/Networking/NetworkInterface.cpp
	kernel/Networking/NetworkManager.cpp
	kernel/Networking/NetworkSocket.cpp
	kernel/Networking/TCPSocket.cpp
	kernel/Networking/UDPSocket.cpp
	kernel/Networking/UNIX/Socket.cpp
	kernel/OpenFileDescriptorSet.cpp
	kernel/Panic.cpp
	kernel/PCI.cpp
	kernel/PIC.cpp
	kernel/Process.cpp
	kernel/Processor.cpp
	kernel/Random.cpp
	kernel/Scheduler.cpp
	kernel/Semaphore.cpp
	kernel/SSP.cpp
	kernel/Storage/ATA/AHCI/Controller.cpp
	kernel/Storage/ATA/AHCI/Device.cpp
	kernel/Storage/ATA/ATABus.cpp
	kernel/Storage/ATA/ATAController.cpp
	kernel/Storage/ATA/ATADevice.cpp
	kernel/Storage/DiskCache.cpp
	kernel/Storage/NVMe/Controller.cpp
	kernel/Storage/NVMe/Namespace.cpp
	kernel/Storage/NVMe/Queue.cpp
	kernel/Storage/Partition.cpp
	kernel/Storage/StorageDevice.cpp
	kernel/Syscall.cpp
	kernel/Terminal/FramebufferTerminal.cpp
	kernel/Terminal/Serial.cpp
	kernel/Terminal/TTY.cpp
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
		arch/x86_64/interrupts.S
		arch/x86_64/PageTable.cpp
		arch/x86_64/Signal.S
		arch/x86_64/Syscall.S
		arch/x86_64/Thread.S
	)
elseif("${BANAN_ARCH}" STREQUAL "i686")
	set(KERNEL_SOURCES
		${KERNEL_SOURCES}
		arch/i686/boot.S
		arch/i686/interrupts.S
		arch/i686/PageTable.cpp
		arch/i686/Signal.S
		arch/i686/Syscall.S
		arch/i686/Thread.S
	)
else()
	message(FATAL_ERROR "unsupported architecure ${BANAN_ARCH}")
endif()

set(BAN_SOURCES
	../BAN/BAN/Assert.cpp
	../BAN/BAN/New.cpp
	../BAN/BAN/StringView.cpp
	../BAN/BAN/Time.cpp
)

set(KLIBC_SOURCES
	klibc/ctype.cpp
	klibc/string.cpp
)

set(LIBELF_SOURCES
	../LibELF/LibELF/LoadableELF.cpp
)

set(LIBINPUT_SOURCE
	../LibInput/KeyboardLayout.cpp
	../LibInput/KeyEvent.cpp
)

set(KERNEL_SOURCES
	${KERNEL_SOURCES}
	${LAI_SOURCES}
	${BAN_SOURCES}
	${KLIBC_SOURCES}
	${LIBELF_SOURCES}
	${LIBINPUT_SOURCE}
)

add_executable(kernel ${KERNEL_SOURCES})
add_dependencies(kernel headers)

target_compile_definitions(kernel PUBLIC __is_kernel)
target_compile_definitions(kernel PUBLIC __arch=${BANAN_ARCH})

target_compile_options(kernel PUBLIC -O2 -g)
target_compile_options(kernel PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix -fno-rtti -fno-exceptions>)
target_compile_options(kernel PUBLIC -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)
target_compile_options(kernel PUBLIC -fstack-protector -ffreestanding -Wall -Wextra -Werror -Wstack-usage=1024 -fno-omit-frame-pointer -mgeneral-regs-only)

# This might not work with other toolchains
target_compile_options(kernel PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>)

if(ENABLE_KERNEL_UBSAN)
	target_compile_options(kernel PUBLIC -fsanitize=undefined)
endif()

if("${BANAN_ARCH}" STREQUAL "x86_64")
	target_compile_options(kernel PUBLIC -mcmodel=kernel -mno-red-zone)
	target_link_options(kernel PUBLIC LINKER:-z,max-page-size=4096)
	target_link_options(kernel PUBLIC LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/linker.ld)
elseif("${BANAN_ARCH}" STREQUAL "i686")
	target_link_options(kernel PUBLIC LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/i686/linker.ld)
endif()

target_link_options(kernel PUBLIC -ffreestanding -nostdlib)

set_source_files_properties(${LAI_SOURCES} PROPERTIES COMPILE_FLAGS -Wno-stack-usage)

add_custom_target(kernel-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_custom_target(kernel-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/kernel ${BANAN_BOOT}/banan-os.kernel
	DEPENDS kernel
)

execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=crtbegin.o OUTPUT_VARIABLE CRTBEGIN OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=crtend.o OUTPUT_VARIABLE CRTEND OUTPUT_STRIP_TRAILING_WHITESPACE)

add_custom_command(
	TARGET kernel PRE_LINK
	COMMAND ${CMAKE_CXX_COMPILER} -MD -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crti.S ${COMPILE_OPTIONS}
	COMMAND ${CMAKE_CXX_COMPILER} -MD -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crtn.S ${COMPILE_OPTIONS}
	COMMAND ${CMAKE_COMMAND} -E copy ${CRTBEGIN} .
	COMMAND ${CMAKE_COMMAND} -E copy ${CRTEND} .
)

#add_custom_command(
#	TARGET kernel POST_BUILD
#	COMMAND x86_64-banan_os-strip ${CMAKE_CURRENT_BINARY_DIR}/kernel
#)

add_custom_command(
	OUTPUT font/prefs.psf.o
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && objcopy -O ${ELF_FORMAT} -B i386 -I binary font/prefs.psf ${CMAKE_CURRENT_BINARY_DIR}/font/prefs.psf.o
)

set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_COMPILER} <CMAKE_CXX_LINK_FLAGS> <FLAGS> <LINK_FLAGS> -o <TARGET> ${CMAKE_CURRENT_BINARY_DIR}/crti.o ${CMAKE_CURRENT_BINARY_DIR}/crtbegin.o <OBJECTS> ${CMAKE_CURRENT_BINARY_DIR}/crtend.o ${CMAKE_CURRENT_BINARY_DIR}/crtn.o -lgcc ")
