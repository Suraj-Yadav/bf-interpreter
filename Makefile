
CXXFLAGS := -Wall -Wextra -Wpedantic -std=c++20 -mavx512bw

build: bfi bfc bfi_g vec

bfi: interpreter.cpp parser.hpp
	g++ -Ofast $(CXXFLAGS) interpreter.cpp -o ./bfi

bfi_g: interpreter.cpp parser.hpp
	g++ -g $(CXXFLAGS) interpreter.cpp -o ./bfi_g

bfc: compiler.cpp parser.hpp
	g++ -g $(CXXFLAGS) compiler.cpp -o ./bfc

vec: vec_test.cpp 
	g++ -O3 $(CXXFLAGS) vec_test.cpp -o vec
	g++ -g  $(CXXFLAGS) vec_test.cpp -o vec_g

clean:
	rm -f bfi bfi_g bfc vec vec_g

test: bfi bfc
	./run_test.sh

test_big: test
	@echo 'Compiler'
	@cd ../bfcheck/ && time ./bfcheck.pl 
	@sed -i 's/run2($$f)/run1($$f)/' ./../bfcheck/bfcheck.pl
	@echo 'Interpreter'
	@cd ../bfcheck/ && time ./bfcheck.pl 
	@sed -i 's/run1($$f)/run2($$f)/' ./../bfcheck/bfcheck.pl

bench: bfi bfc
	@echo 'Compiler'
	hyperfine --warmup 2 \
		--parameter-list loop --no-simple-loop-optimize, \
		--parameter-list scan --no-scan-optimize, \
		--parameter-list input $$(ls -1 ./testing/benches/m*.b | tr '\n' ',' | rev | cut -c 2- | rev ) \
		--setup './bfc {loop} {scan} {input}' \
		'./a.out' \
		--shell=none
	
	@echo 'Interpreter'
	hyperfine --warmup 2 \
		--parameter-list loop --no-simple-loop-optimize, \
		--parameter-list scan --no-scan-optimize, \
		--parameter-list input $$(ls -1 ./testing/benches/m*.b | tr '\n' ',' | rev | cut -c 2- | rev ) \
		'./bfi {loop} {scan} {input}' \
		--shell=none
