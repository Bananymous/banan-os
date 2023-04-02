#!/bin/sh

find . | grep -v 'build' | grep -E '\.(cpp|h|S)$' | xargs wc -l | sort -n
