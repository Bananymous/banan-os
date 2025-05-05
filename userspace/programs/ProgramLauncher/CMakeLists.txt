set(SOURCES
	main.cpp
)

add_executable(ProgramLauncher ${SOURCES})
banan_link_library(ProgramLauncher ban)
banan_link_library(ProgramLauncher libc)
banan_link_library(ProgramLauncher libfont)
banan_link_library(ProgramLauncher libgui)
banan_link_library(ProgramLauncher libinput)

install(TARGETS ProgramLauncher OPTIONAL)
