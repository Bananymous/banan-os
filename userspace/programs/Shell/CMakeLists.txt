set(SOURCES
	main.cpp
	Alias.cpp
	Builtin.cpp
	CommandTypes.cpp
	Execute.cpp
	Input.cpp
	Lexer.cpp
	Token.cpp
	TokenParser.cpp
)

add_executable(Shell ${SOURCES})
banan_link_library(Shell ban)
banan_link_library(Shell libc)

install(TARGETS Shell OPTIONAL)
