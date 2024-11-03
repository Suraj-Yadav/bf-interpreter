
CXXFLAGS := -Wall -Wextra -Wpedantic -std=c++20 -mavx512bw -lgmpxx -lgmp

build:
	cmake --build build -j8

test: build
	./run_test.sh

.PHONY: build

test_big: test
	@echo 'Compiler'
	@cd ../bfcheck/ && time ./bfcheck.pl 
	@sed -i 's/run2($$f)/run1($$f)/' ./../bfcheck/bfcheck.pl
	@echo 'Interpreter'
	@cd ../bfcheck/ && time ./bfcheck.pl 
	@sed -i 's/run1($$f)/run2($$f)/' ./../bfcheck/bfcheck.pl

bench: build
	@echo 'Compiler'
	for i in benches/ma*.b; do \
		hyperfine --warmup 10 \
		--parameter-list loop --no-linearize-loop-optimize, \
		--prepare "./build/bfc {loop} $$i" \
		--command-name="$(basename $$i) {loop}" \
		--export-markdown=- --time-unit=millisecond \
		--shell=none \
		'./a.out'; \
	done

