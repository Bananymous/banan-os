set(SOURCES
	main.cpp
	utils.cpp
)

add_executable(DynamicLoader ${SOURCES})
target_compile_options(DynamicLoader PRIVATE -fno-tree-loop-distribute-patterns -fno-rtti -pie -fpie -ffunction-sections -fdata-sections -mgeneral-regs-only)
target_compile_options(DynamicLoader PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(DynamicLoader PRIVATE __arch=${BANAN_ARCH})
target_link_options(DynamicLoader PRIVATE -nolibc -nostartfiles -static-libgcc -pie -fpie)
target_link_options(DynamicLoader PRIVATE LINKER:--no-dynamic-linker)

# DynamicLoader does not support relocating itself
add_custom_command(
	TARGET DynamicLoader POST_BUILD
	COMMAND readelf --relocs $<TARGET_FILE:DynamicLoader> | grep -q 'There are no relocations in this file'
)

banan_include_headers(DynamicLoader ban)
banan_include_headers(DynamicLoader kernel)
banan_include_headers(DynamicLoader libc)
banan_include_headers(DynamicLoader libelf)

set_target_properties(DynamicLoader PROPERTIES OUTPUT_NAME DynamicLoader.so)
install(TARGETS DynamicLoader DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
