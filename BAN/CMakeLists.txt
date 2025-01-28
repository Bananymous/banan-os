set(BAN_SOURCES
	BAN/Assert.cpp
	BAN/New.cpp
	BAN/StringView.cpp
	BAN/Time.cpp
)

add_library(ban ${BAN_SOURCES})
target_link_options(ban PRIVATE -nolibc)
banan_link_library(ban libc)

set_target_properties(ban PROPERTIES OUTPUT_NAME libban)

# set SONAME as cmake doesn't set it for some reason??
set_target_properties(ban PROPERTIES LINK_FLAGS "-Wl,-soname,libban.so")

banan_install_headers(ban)
install(TARGETS ban OPTIONAL)
