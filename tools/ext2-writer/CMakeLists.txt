cmake_minimum_required(VERSION 3.26)

project(ext2-writer CXX)

set(SOURCES
	../../BAN/BAN/Assert.cpp
	Ext2FileSystem.cpp
	Ext2Inode.cpp
	Inode.cpp
	VirtualRange.cpp

	main.cpp
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(ext2-writer ${SOURCES})
target_compile_options(ext2-writer PRIVATE -O2 -std=c++20)
target_include_directories(ext2-writer PRIVATE include)
target_compile_definitions(ext2-writer PRIVATE stddbg=stderr)
target_compile_definitions(ext2-writer PRIVATE __arch=x86_64)
