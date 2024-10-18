set(SOURCES
	main.cpp
)

add_executable(TaskBar ${SOURCES})
banan_link_library(TaskBar ban)
banan_link_library(TaskBar libc)
banan_link_library(TaskBar libfont)
banan_link_library(TaskBar libgui)
banan_include_headers(TaskBar libinput)

install(TARGETS TaskBar OPTIONAL)
