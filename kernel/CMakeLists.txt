set(KERNEL_SOURCES
	font/prefs.psf.o
	kernel/ACPI/ACPI.cpp
	kernel/ACPI/BatterySystem.cpp
	kernel/ACPI/AML/Namespace.cpp
	kernel/ACPI/AML/Node.cpp
	kernel/ACPI/AML/OpRegion.cpp
	kernel/APIC.cpp
	kernel/BootInfo.cpp
	kernel/CPUID.cpp
	kernel/Credentials.cpp
	kernel/Debug.cpp
	kernel/Device/DebugDevice.cpp
	kernel/Device/Device.cpp
	kernel/Device/FramebufferDevice.cpp
	kernel/Device/NullDevice.cpp
	kernel/Device/RandomDevice.cpp
	kernel/Device/ZeroDevice.cpp
	kernel/ELF.cpp
	kernel/Errors.cpp
	kernel/FS/DevFS/FileSystem.cpp
	kernel/FS/Ext2/FileSystem.cpp
	kernel/FS/Ext2/Inode.cpp
	kernel/FS/FAT/FileSystem.cpp
	kernel/FS/FAT/Inode.cpp
	kernel/FS/FileSystem.cpp
	kernel/FS/Inode.cpp
	kernel/FS/Pipe.cpp
	kernel/FS/ProcFS/FileSystem.cpp
	kernel/FS/ProcFS/Inode.cpp
	kernel/FS/TmpFS/FileSystem.cpp
	kernel/FS/TmpFS/Inode.cpp
	kernel/FS/VirtualFileSystem.cpp
	kernel/GDT.cpp
	kernel/IDT.cpp
	kernel/Input/InputDevice.cpp
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
	kernel/Memory/SharedMemoryObject.cpp
	kernel/Memory/VirtualRange.cpp
	kernel/Networking/ARPTable.cpp
	kernel/Networking/E1000/E1000.cpp
	kernel/Networking/E1000/E1000E.cpp
	kernel/Networking/IPv4Layer.cpp
	kernel/Networking/NetworkInterface.cpp
	kernel/Networking/NetworkLayer.cpp
	kernel/Networking/NetworkManager.cpp
	kernel/Networking/NetworkSocket.cpp
	kernel/Networking/RTL8169/RTL8169.cpp
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
	kernel/ThreadBlocker.cpp
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
	kernel/Storage/SCSI.cpp
	kernel/Storage/StorageDevice.cpp
	kernel/Syscall.cpp
	kernel/Terminal/FramebufferTerminal.cpp
	kernel/Terminal/PseudoTerminal.cpp
	kernel/Terminal/Serial.cpp
	kernel/Terminal/TerminalDriver.cpp
	kernel/Terminal/TextModeTerminal.cpp
	kernel/Terminal/TTY.cpp
	kernel/Terminal/VirtualTTY.cpp
	kernel/Thread.cpp
	kernel/Timer/HPET.cpp
	kernel/Timer/PIT.cpp
	kernel/Timer/RTC.cpp
	kernel/Timer/Timer.cpp
	kernel/USB/Controller.cpp
	kernel/USB/Device.cpp
	kernel/USB/HID/HIDDriver.cpp
	kernel/USB/HID/Keyboard.cpp
	kernel/USB/HID/Mouse.cpp
	kernel/USB/Hub/HubDriver.cpp
	kernel/USB/MassStorage/MassStorageDriver.cpp
	kernel/USB/MassStorage/SCSIDevice.cpp
	kernel/USB/USBManager.cpp
	kernel/USB/XHCI/Controller.cpp
	kernel/USB/XHCI/Device.cpp
	icxxabi.cpp
)

set(ENABLE_KERNEL_UBSAN False)

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

	# Ehhh don't do this but for now libc uses the same stuff kernel can use
	# This won't work after libc starts using sse implemetations tho
	../userspace/libraries/LibC/arch/${BANAN_ARCH}/string.S
)

set(LIBFONT_SOURCES
	../userspace/libraries/LibFont/Font.cpp
	../userspace/libraries/LibFont/PSF.cpp
)

set(LIBINPUT_SOURCE
	../userspace/libraries/LibInput/KeyboardLayout.cpp
	../userspace/libraries/LibInput/KeyEvent.cpp
)

set(KERNEL_SOURCES
	${KERNEL_SOURCES}
	${BAN_SOURCES}
	${KLIBC_SOURCES}
	${LIBFONT_SOURCES}
	${LIBINPUT_SOURCE}
)

add_executable(kernel ${KERNEL_SOURCES})

target_compile_definitions(kernel PRIVATE __is_kernel)
target_compile_definitions(kernel PRIVATE __arch=${BANAN_ARCH})

target_compile_options(kernel PRIVATE
	-O2 -g
	-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.
	-fstack-protector
	-ffreestanding
	-fno-omit-frame-pointer
	-fstrict-volatile-bitfields
	-mgeneral-regs-only
	-Wall -Wextra -Werror -Wstack-usage=1024
)

# C++ specific
target_compile_options(kernel PRIVATE
	-Wno-literal-suffix
	-Wno-invalid-offsetof
	-fno-rtti
	-fno-exceptions
)

if(ENABLE_KERNEL_UBSAN)
	target_compile_options(kernel PRIVATE -fsanitize=undefined)
endif()

if("${BANAN_ARCH}" STREQUAL "x86_64")
	target_compile_options(kernel PRIVATE -mcmodel=kernel -mno-red-zone)
	target_link_options(kernel PRIVATE LINKER:-z,max-page-size=4096)
	target_link_options(kernel PRIVATE LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/linker.ld)
elseif("${BANAN_ARCH}" STREQUAL "i686")
	target_link_options(kernel PRIVATE LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/i686/linker.ld)
endif()

target_link_options(kernel PRIVATE -ffreestanding -nostdlib -orphan-handling=error)

get_target_property(KERNEL_COMPILE_OPTIONS kernel COMPILE_OPTIONS)
execute_process(COMMAND ${CMAKE_CXX_COMPILER} ${KERNEL_COMPILE_OPTIONS} -print-file-name=crtbegin.o OUTPUT_VARIABLE CRTBEGIN OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${CMAKE_CXX_COMPILER} ${KERNEL_COMPILE_OPTIONS} -print-file-name=crtend.o OUTPUT_VARIABLE CRTEND OUTPUT_STRIP_TRAILING_WHITESPACE)

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

banan_include_headers(kernel ban)
banan_include_headers(kernel libc)
banan_include_headers(kernel libfont)
banan_include_headers(kernel libelf)
banan_include_headers(kernel libinput)

banan_install_headers(kernel)
set_target_properties(kernel PROPERTIES OUTPUT_NAME banan-os.kernel)
install(TARGETS kernel DESTINATION ${BANAN_BOOT} OPTIONAL)

if("${BANAN_ARCH}" STREQUAL "x86_64")
	set(ELF_FORMAT elf64-x86-64)
elseif("${BANAN_ARCH}" STREQUAL "i686")
	set(ELF_FORMAT elf32-i386)
endif()

add_custom_command(
	OUTPUT font/prefs.psf.o
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/font && objcopy -O ${ELF_FORMAT} -B i386 -I binary font/prefs.psf ${CMAKE_CURRENT_BINARY_DIR}/font/prefs.psf.o
)

set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_COMPILER} <CMAKE_CXX_LINK_FLAGS> <FLAGS> <LINK_FLAGS> -o <TARGET> ${CMAKE_CURRENT_BINARY_DIR}/crti.o ${CMAKE_CURRENT_BINARY_DIR}/crtbegin.o <OBJECTS> ${CMAKE_CURRENT_BINARY_DIR}/crtend.o ${CMAKE_CURRENT_BINARY_DIR}/crtn.o -lgcc ")
