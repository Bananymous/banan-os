#!/bin/bash

find . | grep -vE '(build|toolchain)' | grep -E '\.(cpp|h|S)$' | xargs wc -l | sort -n
