set(SOURCES
	main.cpp
)

add_executable(ImageViewer ${SOURCES})
banan_link_library(ImageViewer ban)
banan_link_library(ImageViewer libc)
banan_link_library(ImageViewer libgui)
banan_link_library(ImageViewer libimage)

install(TARGETS ImageViewer OPTIONAL)
