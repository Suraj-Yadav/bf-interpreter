
CXXFLAGS := -Wall -Wextra -Wpedantic -std=c++20 -mavx512bw -lgmpxx -lgmp

build: bfi bfc bfi_g vec

bfi: interpreter.cpp parser.hpp math.hpp util.hpp
	g++ -Ofast $(CXXFLAGS) interpreter.cpp -o ./bfi

bfi_g: interpreter.cpp parser.hpp math.hpp util.hpp
	g++ -g $(CXXFLAGS) interpreter.cpp -o ./bfi_g

bfc: compiler.cpp parser.hpp math.hpp util.hpp
	g++ -g $(CXXFLAGS) compiler.cpp -o ./bfc

vec: vec_test.cpp util.hpp
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
	for i in benches/*.b; do \
		./bfc $$i; \
		hyperfine --warmup 10 --parameter-list loop --no-linearize-loop-optimize, --prepare "./bfc {loop} $$i" './a.out' --command-name="$(basename $$i) {loop}" --export-markdown=- --time-unit=millisecond --shell=none; \
	done

