#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <stack>
#include <vector>

using DATA_TYPE = unsigned char;

class Tape {
	static_assert(sizeof(DATA_TYPE) == 1);
	struct Cell {
		DATA_TYPE val = 0;
		bool isConst = true;
	};
	std::vector<Cell> right, left;
	int index = 0;

   public:
	Tape() { right.emplace_back(); }

	void print() {
		std::cout << "index = " << index << '\n';
		while (!left.empty()) {
			const auto& e = left.back();
			std::cout << (e.isConst ? '*' : ' ') << (int)e.val << "\t";
			left.pop_back();
		}
		for (auto& e : right) {
			std::cout << (e.isConst ? '*' : ' ') << (int)e.val << "\t";
		}
		std::cout << '\n';
	}

	DATA_TYPE& get(int delta = 0) { return operator[](index + delta).val; }
	bool& isConst(int delta = 0) { return operator[](index + delta).isConst; }

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

	Cell& operator[](int index) {
		expand(index);
		if (index >= 0) { return right[index]; }
		index = -index - 1;
		return left[index];
	}

	int begin() { return -static_cast<int>(left.size()); }
	int end() { return static_cast<int>(right.size()); }
};

enum Inst_Codes {
	NO_OP = 0,
	TAPE_M,	 // Tape Movement
	INCR_C,	 // Increment by constant
	INCR_R,	 // Increment by reference (reference is relative)
	WRITE,
	READ,
	JUMP_C,	 // Jump to closing bracket
	JUMP_O,	 // Jump to opening bracket
	HALT,
};

struct Instruction {
	Inst_Codes code = Inst_Codes::NO_OP;
	int lRef = 0, rRef = 0;
	int value = 0;
};

#define LOG_INST 1

#ifdef LOG_INST
std::ostream& operator<<(std::ostream& os, const Instruction& a) {
	constexpr auto InstNames = std::to_array(
		{"NO_OP", "TAPE_M", "INCR_C", "INCR_R", "WRITE", "READ", "JUMP_C",
		 "JUMP_O", "HALT"});

	static_assert((Inst_Codes::HALT + 1) == InstNames.size());

	return os << InstNames[a.code] << "\t" << a.lRef << " = " << a.rRef << ", "
			  << a.value;
}
#endif

bool operator<(const Instruction& a, const Instruction& b) {
	if (a.code != b.code) { return a.code < b.code; }
	return a.lRef < b.lRef;
}

void run(
	Tape& tape, bool enableIO, std::vector<Instruction>::const_iterator begin,
	std::vector<Instruction>::const_iterator end) {
	for (auto itr = begin; itr != end; itr++) {
		const auto& inst = *itr;
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
				if (enableIO) { std::cout << tape.get(); }
				break;
			}
			case READ: {
				if (enableIO) { std::cin >> tape.get(); }
				tape.isConst() = false;
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
				if (inst.lRef == inst.rRef && inst.value == -1) {
					tape.isConst(inst.lRef) = true;
				} else {
					tape.isConst(inst.lRef) = tape.isConst(inst.rRef);
				}
				tape.get(inst.lRef) += inst.value * tape.get(inst.rRef);
				break;
			}
			case NO_OP:
				break;
			case HALT:
				return;
		}
	}
}

class Program {
	std::vector<Instruction> program;

	void aggregate() {
		if (program.size() < 2) { return; }
		auto& b = program.back();
		auto& a = program[program.size() - 2];
		if (a.code == b.code && a.code == Inst_Codes::INCR_C &&
			a.lRef == b.lRef) {
			a.value += b.value;
			program.pop_back();
			return;
		}
		if (a.code == b.code && a.code == Inst_Codes::TAPE_M) {
			a.value += b.value;
			program.pop_back();
			return;
		}
	}

	void parse(const std::string& file) {
		std::ifstream input(file);
		std::stack<int> stack;

#ifdef LOG_INST
		std::ofstream original("/tmp/orig.bfas");
#endif

		while (true) {
			auto ch = '\0';
			Instruction inst{NO_OP};
			if (!(input >> ch)) { inst = {Inst_Codes::HALT, 0, 0, 0}; }
			switch (ch) {
				case '>': {
					inst = {Inst_Codes::TAPE_M, 0, 0, +1};
					break;
				}
				case '<': {
					inst = {Inst_Codes::TAPE_M, 0, 0, -1};
					break;
				}
				case '+': {
					inst = {Inst_Codes::INCR_C, 0, 0, +1};
					break;
				}
				case '-': {
					inst = {Inst_Codes::INCR_C, 0, 0, -1};
					break;
				}
				case '.': {
					inst = {Inst_Codes::WRITE, 0, 0, 0};
					break;
				}
				case ',': {
					inst = {Inst_Codes::READ, 0, 0, 0};
					break;
				}
				case '[': {
					inst = {Inst_Codes::JUMP_C, 0, 0, 0};
					break;
				}
				case ']': {
					inst = {Inst_Codes::JUMP_O, 0, 0, 0};
					break;
				}
				default:
					break;
			}

			if (inst.code != NO_OP) {
				program.push_back(inst);
#ifdef LOG_INST
				original << inst << "\n";
#endif
				aggregate();
			}
			if (inst.code == JUMP_C) {
				int pos = static_cast<int>(program.size() - 1);
				stack.push(pos);
			} else if (inst.code == JUMP_O) {
				int closing = static_cast<int>(program.size() - 1);
				int opening = stack.top();
				program[opening].value = closing - opening;
				program[closing].value = opening - closing;
				stack.pop();
			} else if (inst.code == HALT) {
				return;
			}
		}
	}

   public:
	Program(const std::string& file) {
		parse(file);
#ifdef LOG_INST
		std::ofstream optimized("/tmp/actual.bfas");
		for (const auto& i : program) { optimized << i << "\n"; }
#endif
	}
	const auto& instructions() { return program; }
};

int main(int argc, char* argv[]) {
	Tape tape;
	std::string file = "./input.txt";
	if (argc > 1) { file = argv[1]; }

	Program p(file);

	const auto& code = p.instructions();

	run(tape, true, code.cbegin(), code.cend());

	return 0;
}
