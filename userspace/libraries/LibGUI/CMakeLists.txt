set(LIBGUI_SOURCES
	Texture.cpp
	Widget/Button.cpp
	Widget/Grid.cpp
	Widget/Label.cpp
	Widget/RoundedWidget.cpp
	Widget/TextArea.cpp
	Widget/Widget.cpp
	Window.cpp
)

add_library(libgui ${LIBGUI_SOURCES})
banan_link_library(libgui ban)
banan_link_library(libgui libc)
banan_link_library(libgui libfont)
banan_link_library(libgui libinput)

banan_install_headers(libgui)
install(TARGETS libgui OPTIONAL)
