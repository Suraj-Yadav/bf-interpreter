#!/bin/bash

for file in ./benches/*.b; do
	printf "\nRunning ${file}"
	time ./bf ${file} >./run.out || exit 1
	if ! diff --brief ./run.out ${file}.out.txt; then
		wc -c  ${file}
		echo for details compare ./run.out ${file}.out.txt
		exit 1
	fi
	rm ./run.out
done
