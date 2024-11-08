#!/bin/bash

BLACK=$(tput setaf 0)
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
LIME_YELLOW=$(tput setaf 190)
POWDER_BLUE=$(tput setaf 153)
BLUE=$(tput setaf 4)
MAGENTA=$(tput setaf 5)
CYAN=$(tput setaf 6)
WHITE=$(tput setaf 7)
BRIGHT=$(tput bold)
NORMAL=$(tput sgr0)
BLINK=$(tput blink)
REVERSE=$(tput smso)
UNDERLINE=$(tput smul)

verify() {
	file=$1
	if ! diff --brief ./run.out ${file}.out.txt; then
		echo "${RED}FAILED: ${file}${NORMAL}"
		echo "Compare ./run.out ${file}.out.txt"
		exit 1
	else
		echo "${GREEN}PASSED: ${file}${NORMAL}"
	fi
}

echo "Running Test for compiler"
for file in ./benches/*.b; do
	./build/bfc ${file}
	timeout --verbose 20 ./a.out >./run.out
	verify "${file}"
	rm ./a.out ./run.out
done

echo
echo "Running Test for Interpreter"
for file in ./benches/*.b; do
	timeout --verbose 20 ./build/bfi ${file} >./run.out
	verify "${file}"
	rm ./run.out
done
