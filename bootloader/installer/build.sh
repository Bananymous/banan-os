#!/bin/sh

g++ -O2 -std=c++20 main.cpp crc32.cpp ELF.cpp GPT.cpp GUID.cpp -o install-bootloader
