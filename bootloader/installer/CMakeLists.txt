cmake_minimum_required(VERSION 3.26)

project(x86_64-banan_os-bootloader-installer CXX)

set(SOURCES
    crc32.cpp
    ELF.cpp
    GPT.cpp
    GUID.cpp
	main.cpp
)

add_executable(x86_64-banan_os-bootloader-installer ${SOURCES})
target_compile_options(x86_64-banan_os-bootloader-installer PRIVATE -O2 -std=c++20)
target_compile_definitions(x86_64-banan_os-bootloader-installer PRIVATE __arch=x86_64)
target_include_directories(x86_64-banan_os-bootloader-installer PRIVATE ${CMAKE_SOURCE_DIR}/../../LibELF/include)
target_include_directories(x86_64-banan_os-bootloader-installer PRIVATE ${CMAKE_SOURCE_DIR}/../../kernel/include)
