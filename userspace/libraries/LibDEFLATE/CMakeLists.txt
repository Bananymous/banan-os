set(LIBDEFLATE_SOURCES
	Compressor.cpp
	Decompressor.cpp
	HuffmanTree.cpp
)

add_library(libdeflate ${LIBDEFLATE_SOURCES})
banan_link_library(libdeflate ban)
banan_link_library(libdeflate libc)

banan_install_headers(libdeflate)
install(TARGETS libdeflate OPTIONAL)
