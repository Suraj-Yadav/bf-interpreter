bf: main.cpp 
	g++ -Ofast main.cpp -o bf

build: bf

clean:
	rm bf

test: build
	@for file in ./benches/*.b ; do \
		printf "\nRunning $${file}"; \
		time ./bf $${file} >./run.out || exit 1; \
		diff --brief ./run.out $${file}.out.txt || exit 1; \
	done
	@rm ./run.out

