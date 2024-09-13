build: bf

bf: main.cpp 
	g++ -Ofast -std=c++20 main.cpp -o ./bf


clean:
	rm bf

test: build
	@for file in ./benches/*.b ; do \
		printf "\nRunning $${file}"; \
		time ./bf $${file} >./run.out || exit 1; \
		diff --brief ./run.out $${file}.out.txt || exit 1; \
	done
	@rm ./run.out

