set(SOURCES
	main.cpp
)

add_executable(test ${SOURCES})
banan_link_library(test ban)

install(TARGETS test OPTIONAL)

install(CODE "
	if (EXISTS ${CMAKE_INSTALL_BINDIR}/test)
		file(COPY_FILE ${CMAKE_INSTALL_BINDIR}/test ${CMAKE_INSTALL_BINDIR}/[ ONLY_IF_DIFFERENT)
	endif()
")
