#!/bin/sh

find . | grep -v 'sysroot' | grep -E '\.(cpp|h|S)$' | xargs wc -l | sort -n
