#!/bin/bash

set -e

PROGRAM_NAME=$1

mkdir $PROGRAM_NAME

cat > $PROGRAM_NAME/CMakeLists.txt << EOF
cmake_minimum_required(VERSION 3.26)

project($PROGRAM_NAME CXX)

set(SOURCES
	main.cpp
)

add_executable($PROGRAM_NAME \${SOURCES})
target_compile_options($PROGRAM_NAME PUBLIC -O2 -g)
add_dependencies($PROGRAM_NAME libc-install)
target_link_options($PROGRAM_NAME PUBLIC -nodefaultlibs)
target_link_libraries($PROGRAM_NAME PUBLIC \${BANAN_LIB}/libc.a)

add_custom_target($PROGRAM_NAME-install
	COMMAND cp \${CMAKE_CURRENT_BINARY_DIR}/$PROGRAM_NAME \${BANAN_BIN}/
	DEPENDS $PROGRAM_NAME
)
EOF

cat > $PROGRAM_NAME/main.cpp << EOF
int main()
{

}
EOF
