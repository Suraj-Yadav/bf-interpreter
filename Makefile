build: bf

bf: main.cpp 
	g++ -Ofast -Wall -Wextra -Wpedantic -std=c++20 main.cpp -o ./bf

build_dbg: bf_g build

bf_g: main.cpp
	g++ -g -std=c++20 main.cpp -o ./bf_g

clean:
	rm bf

test: build
	./run_test.sh

