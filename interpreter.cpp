#include <iostream>
#include <span>
#include <vector>

#include "parser.hpp"

class Tape {
	static_assert(sizeof(DATA_TYPE) == 1);
	std::vector<DATA_TYPE> right, left;
	int index = 0;

   public:
	Tape() { right.emplace_back(); }

	void print() {
		std::cout << "index = " << index << '\n';
		while (!left.empty()) {
			const auto& e = left.back();
			std::cout << (int)e << "\t";
			left.pop_back();
		}
		for (auto& e : right) { std::cout << (int)e << "\t"; }
		std::cout << '\n';
	}

	DATA_TYPE& get(int delta = 0) { return operator[](index + delta); }

	void expand(int ind) {
		if (ind >= 0) {
			while (ind >= static_cast<int>(right.size())) {
				right.emplace_back();
			}
		} else {
			auto i = -ind - 1;
			while (i >= static_cast<int>(left.size())) { left.emplace_back(); }
		}
	}

	void move(int delta) {
		index += delta;
		expand(index);
	}

	void moveRight() { move(1); }

	void moveLeft() { move(-1); }

	DATA_TYPE& operator[](int index) {
		expand(index);
		if (index >= 0) { return right[index]; }
		index = -index - 1;
		return left[index];
	}

	int begin() { return -static_cast<int>(left.size()); }
	int end() { return static_cast<int>(right.size()); }
};

auto run(Tape& tape, std::span<Instruction> code) {
	std::vector<int> count(code.size(), 0);
	for (auto itr = code.begin(); itr != code.end(); itr++) {
		const auto& inst = *itr;
		count[itr - code.begin()]++;
		switch (inst.code) {
			case TAPE_M: {
				tape.move(inst.value);
				break;
			}
			case INCR_C: {
				tape.get(inst.lRef) += inst.value;
				break;
			}
			case WRITE: {
				std::cout << tape.get();
				break;
			}
			case READ: {
				char ch = '\0';
				std::cin.get(ch);
				tape.get() = ch;
				break;
			}
			case JUMP_C: {
				if (tape.get() == 0) {
					itr += inst.value;
					itr--;
				}
				break;
			}
			case JUMP_O: {
				if (tape.get() != 0) {
					itr += inst.value;
					itr--;
				}
				break;
			}
			case INCR_R: {
				tape.get(inst.lRef) += inst.value * tape.get(inst.rRef);
				break;
			}
			case DEBUG: {
				tape.print();
			}
			case NO_OP:
				break;
			case HALT:
				return count;
		}
	}
	return count;
}

int main(int argc, char* argv[]) {
	std::string file;
	bool profile = false;
	std::vector<std::string> args(argv + 1, argv + argc);
	for (auto& arg : args) {
		if (arg == "-p") {
			profile = true;
		} else if (file.empty()) {
			file = arg;
		}
	}

	if (file.empty()) {
		std::cerr << "bf: fatal error: no input files\n";
		return 1;
	}

	Program p(file);

	if (!p.isOK()) {
		std::cerr << p.error() << "\n";
		return 1;
	}

	auto& code = p.instructions();

	Tape tape;
	if (profile) {
		auto counts = run(tape, code);
		p.printProfileInfo(counts);
	} else {
		run(tape, code);
	}

	return 0;
}
