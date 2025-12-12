#!/bin/bash

if [[ -z $1 ]]; then
	echo Please specify day number >& 2
	exit 1
fi

if [[ ! -d day$1 ]]; then
	cp -r day-template day$1
	sed -i "s/day-template/day$1/g" day$1/*
fi

if [[ ! -f input/day$1_input.txt ]]; then
	wget --no-cookies --header "Cookie: session=$AOC_SESSION" https://adventofcode.com/2025/day/$1/input -O input/day$1_input.txt
fi
