#include <fstream>
#include <iostream>
#include <stack>
#include <vector>

using DATA_TYPE = unsigned char;

class Tape {
	static_assert(sizeof(DATA_TYPE) == 1);

	std::vector<DATA_TYPE> right, left;
	int index = 0;

   public:
	Tape() { right.push_back(0); }

	void print() {
		std::cout << "index = " << index << '\n';
		while (!left.empty()) {
			std::cout << (int)left.back() << "\t";
			left.pop_back();
		}
		for (auto& e : right) { std::cout << (int)e << "\t"; }
		std::cout << '\n';
	}

	DATA_TYPE& get() { return operator[](index); }

	void move(int delta) {
		index += delta;
		if (index >= 0) {
			while (index >= right.size()) { right.push_back(0); }
		} else {
			auto i = -index - 1;
			while (i >= left.size()) { left.push_back(0); }
		}
	}

	void moveRight() {
		index++;
		if (index == right.size()) { right.push_back(0); }
	}

	void moveLeft() {
		index--;
		auto i = -index - 1;
		if (i == left.size()) { left.push_back(0); }
	}

	DATA_TYPE& operator[](int index) {
		if (index >= 0) { return right[index]; }
		index = -index - 1;
		return left[index];
	}
};

class Jumps {
	std::vector<int> jumps;

   public:
	void add() { jumps.push_back(-1); }
	void addLeft(int l) {}
	void addRight(int l, int r) {
		jumps[r] = l;
		jumps[l] = r;
	}

	int getJump(int p) { return jumps[p]; }
};

constexpr std::string_view ALLOWED_CHARS = "><+-[].,$";

enum Inst_Codes {
	NO_OP = 0,
	TAPE_M,	 // Tape Movement
	INCR_C,	 // Increment by constant
	INCR_R,	 // Increment by reference
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

// #define LOG_INST 1

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

class Program {
	std::vector<Instruction> program;
	int p = 0;
	std::ifstream input;
	std::stack<int> stack;
	Jumps jumps;

#ifdef LOG_INST
	std::ofstream original("/tmp/orig.bfas");
#endif

	void addInstruction(Instruction inst) {
		program.push_back(inst);
		jumps.add();
#ifdef LOG_INST
		original << inst << "\n";
#endif
	}

	void parse() {
		while (true) {
			auto ch = '\0';
			if (!(input >> ch)) {
				addInstruction({HALT});
				return;
			}
			switch (ch) {
				case '>': {
					addInstruction({Inst_Codes::TAPE_M, 0, 0, +1});
					break;
				}
				case '<': {
					addInstruction({Inst_Codes::TAPE_M, 0, 0, -1});
					break;
				}
				case '+': {
					addInstruction({Inst_Codes::INCR_C, 0, 0, +1});
					break;
				}
				case '-': {
					addInstruction({Inst_Codes::INCR_C, 0, 0, -1});
					break;
				}
				case '.': {
					addInstruction({Inst_Codes::WRITE, 0, 0, 0});
					break;
				}
				case ',': {
					addInstruction({Inst_Codes::READ, 0, 0, 0});
					break;
				}
				case '[': {
					int pos = static_cast<int>(program.size());
					stack.push(pos);
					addInstruction({Inst_Codes::JUMP_C, 0, 0, 0});
					jumps.addLeft(pos);
					break;
				}
				case ']': {
					int pos = static_cast<int>(program.size());
					addInstruction({Inst_Codes::JUMP_O, 0, 0, 0});
					jumps.addRight(stack.top(), pos);
					stack.pop();
					break;
				}
				default:
					break;
			}
		}
	}

   public:
	Program(const std::string& file) : input(file) { parse(); }
	~Program() {
#ifdef LOG_INST
		std::ofstream optimized("/tmp/actual.bfas");
		for (const auto& i : program) { optimized << i << "\n"; }
#endif
	}

	Instruction get() { return program[p]; }

	void next() { p++; }

	void jump() {
		auto tempP = p;
		while (jumps.getJump(tempP) == -1) {
			get();
			next();
		}
		p = jumps.getJump(tempP);
	}
};

int main(int argc, char* argv[]) {
	char ch = 0;
	Tape tape;
	std::string file = "./input.txt";
	if (argc > 1) { file = argv[1]; }

	Program p(file);

	while (true) {
		const auto inst = p.get();
		if (inst.code == Inst_Codes::HALT) { break; }
		switch (inst.code) {
			case TAPE_M: {
				tape.move(inst.value);
				p.next();
				break;
			}
			case INCR_C: {
				tape.get() += inst.value;
				p.next();
				break;
			}
			case WRITE: {
				std::cout << tape.get();
				p.next();
				break;
			}
			case READ: {
				std::cin >> tape.get();
				p.next();
				break;
			}
			case JUMP_C: {
				if (tape.get() == 0) {
					p.jump();
				} else {
					p.next();
				}
				break;
			}
			case JUMP_O: {
				if (tape.get() != 0) {
					p.jump();
				} else {
					p.next();
				}
				break;
			}
			case HALT:
			case NO_OP:
			case INCR_R:
				p.next();
				break;
		}
	}

	return 0;
}
