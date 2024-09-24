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
	seconds=$2
	if ! diff --brief ./run.out ${file}.out.txt; then
		echo "${RED}FAILED: ${file}${NORMAL}"
		echo "Compare ./run.out ${file}.out.txt"
		exit 1
	else
		echo "${GREEN}PASSED: ${file} ${seconds} secs ${NORMAL}"
	fi
}

echo "Running Test for compiler"
for file in ./benches/*.b; do
	./bfc ${file}
	SECONDS=0
	timeout 20 ./a.out >./run.out
	verify "${file}" $SECONDS
	rm ./a.out ./run.out
done

echo "Running Test for Interpreter"
for file in ./benches/*.b; do
	SECONDS=0
	timeout 20 ./bfi ${file} >./run.out
	verify "${file}" $SECONDS
	rm ./run.out
done
