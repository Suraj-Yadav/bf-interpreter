
CXXFLAGS := -Wall -Wextra -Wpedantic -std=c++20

build: bfi bfc

build_dbg: bfi_g bfc_g build

bfi: interpreter.cpp 
	g++ -Ofast $(CXXFLAGS) interpreter.cpp -o ./bfi

bfi_g: interpreter.cpp
	g++ -g $(CXXFLAGS) interpreter.cpp -o ./bfi_g

bfc: compiler.cpp
	g++ -g $(CXXFLAGS) compiler.cpp -o ./bfc

clean:
	rm -f bfi bfi_g bfc

test: bfi bfc
	./run_test.sh

