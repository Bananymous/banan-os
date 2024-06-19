set(BAN_SOURCES
	BAN/Assert.cpp
	BAN/New.cpp
	BAN/StringView.cpp
	BAN/Time.cpp
)

add_library(ban ${BAN_SOURCES})
banan_link_library(ban libc)

banan_install_headers(ban)
install(TARGETS ban)
