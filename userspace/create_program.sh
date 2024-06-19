#!/bin/bash

set -e

PROGRAM_NAME=$1

mkdir $PROGRAM_NAME

cat > $PROGRAM_NAME/CMakeLists.txt << EOF
set(SOURCES
	main.cpp
)

add_executable($PROGRAM_NAME \${SOURCES})
banan_link_library($PROGRAM_NAME ban)
banan_link_library($PROGRAM_NAME libc)

install(TARGETS $PROGRAM_NAME)
EOF

cat > $PROGRAM_NAME/main.cpp << EOF
#include <stdio.h>

int main()
{
	printf("Hello World\n");
}
EOF
