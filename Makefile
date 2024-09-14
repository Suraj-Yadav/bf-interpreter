build: bf

bf: main.cpp 
	g++ -Ofast -Wall -Wextra -Wpedantic -std=c++20 main.cpp -o ./bf

build_dbg: bf_g build

bf_g: main.cpp
	g++ -g -std=c++20 main.cpp -o ./bf_g

clean:
	rm bf

test: build
	@for file in ./benches/*.b ; do \
		printf "\nRunning $${file}"; \
		time ./bf $${file} >./run.out || exit 1; \
		diff --brief ./run.out $${file}.out.txt || exit 1; \
	done
	@rm ./run.out

