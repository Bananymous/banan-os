cmake_minimum_required(VERSION 3.26)

if (NOT DEFINED ENV{BANAN_ARCH})
	message(FATAL_ERROR "environment variable BANAN_ARCH not defined")
endif ()
set(BANAN_ARCH $ENV{BANAN_ARCH})

project(banan_os-bootloader-installer CXX)

set(SOURCES
    crc32.cpp
    ELF.cpp
    GPT.cpp
    GUID.cpp
	main.cpp
)

add_executable(banan_os-bootloader-installer ${SOURCES})
target_compile_options(banan_os-bootloader-installer PRIVATE -O2 -std=c++20)
target_compile_definitions(banan_os-bootloader-installer PRIVATE __arch=${BANAN_ARCH})
target_include_directories(banan_os-bootloader-installer PRIVATE ${CMAKE_SOURCE_DIR}/../../LibELF/include)
target_include_directories(banan_os-bootloader-installer PRIVATE ${CMAKE_SOURCE_DIR}/../../kernel/include)
