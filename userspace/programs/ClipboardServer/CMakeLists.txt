set(SOURCES
	main.cpp
)

add_executable(ClipboardServer ${SOURCES})
banan_include_headers(ClipboardServer libclipboard)
banan_link_library(ClipboardServer ban)
banan_link_library(ClipboardServer libc)

install(TARGETS ClipboardServer OPTIONAL)
