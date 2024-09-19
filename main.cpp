#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stack>
#include <vector>

using DATA_TYPE = unsigned char;

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

enum Inst_Codes {
	NO_OP = 0,
	TAPE_M,	 // Tape Movement
	INCR_C,	 // Increment by constant
	INCR_R,	 // Increment by reference (reference is relative)
	WRITE,
	READ,
	JUMP_C,	 // Jump to closing bracket
	JUMP_O,	 // Jump to opening bracket
	DEBUG,
	HALT,
};

struct Instruction {
	Inst_Codes code = Inst_Codes::NO_OP;
	int lRef = 0, rRef = 0;
	int value = 0;
};

Instruction getInstruction(char ch) {
	switch (ch) {
		case '>':
			return {Inst_Codes::TAPE_M, 0, 0, +1};
		case '<':
			return {Inst_Codes::TAPE_M, 0, 0, -1};
		case '+':
			return {Inst_Codes::INCR_C, 0, 0, +1};
		case '-':
			return {Inst_Codes::INCR_C, 0, 0, -1};
		case '.':
			return {Inst_Codes::WRITE, 0, 0, 0};
		case ',':
			return {Inst_Codes::READ, 0, 0, 0};
		case '[':
			return {Inst_Codes::JUMP_C, 0, 0, 0};
		case ']':
			return {Inst_Codes::JUMP_O, 0, 0, 0};
		case '$':
			return {Inst_Codes::DEBUG, 0, 0, 0};
	}
	return {Inst_Codes::NO_OP, 0, 0, 0};
}

#define LOG_INST 1

#ifdef LOG_INST
std::ostream& operator<<(std::ostream& os, const Instruction& a) {
	constexpr auto InstNames = std::to_array(
		{"NO_OP", "TAPE_M", "INCR_C", "INCR_R", "WRITE", "READ", "JUMP_C",
		 "JUMP_O", "DEBUG", "HALT"});

	static_assert((Inst_Codes::HALT + 1) == InstNames.size());

	return os << InstNames[a.code] << "\t" << a.lRef << " = " << a.rRef << ", "
			  << a.value;
}
#endif

bool operator<(const Instruction& a, const Instruction& b) {
	if (a.code != b.code) { return a.code < b.code; }
	return a.lRef < b.lRef;
}

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

// Determine loop count (i) by solving (a + d * i) mod 256 = 0 (mod 256)
bool solveForLoopCount(DATA_TYPE a, DATA_TYPE d, DATA_TYPE& i) {
	for (i = 1; i <= std::numeric_limits<DATA_TYPE>::max(); i++) {
		a += d;
		if (a == 0) { return true; }
	}
	return false;
}

bool isLoop(std::span<Instruction> code) {
	if (code.empty()) { return false; }
	if (code.front().code != JUMP_C) { return false; }
	if (code.back().code != JUMP_O) { return false; }
	code = code.subspan(1, code.size() - 2);
	return !code.empty();
}

bool isInnerMostLoop(std::span<Instruction> code) {
	if (!isLoop(code)) { return false; }
	code = code.subspan(1, code.size() - 2);
	for (auto& e : code) {
		if (e.code == JUMP_C) { return false; }
	}
	return true;
}

bool isSimpleLoop(std::span<Instruction> code) {
	if (!isLoop(code)) { return false; }
	code = code.subspan(1, code.size() - 2);

	Tape tape;
	for (auto& e : code) {
		switch (e.code) {
			case INCR_R:
			case WRITE:
			case READ:
			case DEBUG:
			case HALT:
				return false;
			case JUMP_C:
			case JUMP_O:
			case NO_OP:
				break;
			case TAPE_M:
				tape.move(e.value);
				break;
			case INCR_C:
				tape.get(e.lRef) += e.value;
				break;
		}
	}
	return tape.get() == 1 ||
		   tape.get() == std::numeric_limits<DATA_TYPE>::max();
}

class Program {
	std::optional<std::string> err;
	std::vector<Instruction> program;
	std::string sourceCode;
	std::vector<int> srcToProgram;

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

	int getProgramToCode(int pos) {
		return static_cast<int>(
			std::lower_bound(srcToProgram.begin(), srcToProgram.end(), pos) -
			srcToProgram.begin());
	}

	void parse(const std::string& file) {
		std::ifstream input(file);
		if (!input.is_open()) {
			err = "Cannot read file: '" + file + "'";
			return;
		}
		std::stack<int> stack;

#ifdef LOG_INST
		std::ofstream original("/tmp/orig.bfas");
#endif

		while (true) {
			auto ch = '\0';
			Instruction inst;

			if (input.get(ch)) {
				inst = getInstruction(ch);
			} else {
				inst = {Inst_Codes::HALT, 0, 0, 0};
			}

			if (inst.code != NO_OP) {
				program.push_back(inst);
#ifdef LOG_INST
				original << inst << "\n";
#endif
				aggregate();
			} else {
				ch = '\0';
			}

			sourceCode.push_back(ch);
			srcToProgram.push_back(static_cast<int>(program.size() - 1));

			if (inst.code == JUMP_C) {
				int pos = static_cast<int>(program.size() - 1);
				stack.push(pos);
			} else if (inst.code == JUMP_O) {
				int closing = static_cast<int>(program.size() - 1);
				if (stack.empty()) {
					err = "Mismatched loop end at char " +
						  std::to_string(getProgramToCode(closing));
					break;
				}
				int opening = stack.top();
				program[opening].value = closing - opening;
				program[closing].value = opening - closing;
				stack.pop();
			} else if (inst.code == HALT) {
				break;
			}
		}
		if (!stack.empty()) {
			err = "Mismatched loop start at char " +
				  std::to_string(getProgramToCode(stack.top()));
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
	[[nodiscard]] auto isOK() const { return !err.has_value(); }
	auto error() { return err.value(); }
	auto& instructions() { return program; }

	void printProfileInfo(std::span<int> runCounts) {
		constexpr auto WIDTH = 5;
		if (runCounts.size() != program.size()) {
			throw std::runtime_error(
				"Mismatch in length of runCounts and program");
		}
		std::cout << "\n==============Profile Info==============\n";
		for (auto i = 0u; i < sourceCode.size(); ++i) {
			if (sourceCode[i] == '\0') { continue; }
			std::cout << std::setw(WIDTH) << i << " : " << sourceCode[i]
					  << " : " << runCounts[srcToProgram[i]] << "\n";
		}
		std::vector<std::pair<int, int>> simple, notSimple;
		for (auto i = 0u; i < program.size(); ++i) {
			const auto& instr = program[i];
			if (instr.code != JUMP_C) { continue; }
			std::span<Instruction> code(
				program.begin() + i, program.begin() + i + instr.value + 1);
			if (!isInnerMostLoop(code)) { continue; }
			if (isSimpleLoop(code)) {
				simple.emplace_back(runCounts[i], i);
			} else {
				notSimple.emplace_back(runCounts[i], i);
			}
		}
		std::sort(simple.begin(), simple.end(), std::greater<>());
		std::sort(notSimple.begin(), notSimple.end(), std::greater<>());
		if (!simple.empty()) {
			std::cout << "\n==============Simple Loops==============\n";
		}
		for (auto& e : simple) {
			auto begin = getProgramToCode(e.second);
			std::cout << std::setw(WIDTH) << e.second << " : ";
			for (auto i = begin; sourceCode[i] != ']'; ++i) {
				std::cout << sourceCode[i];
			}
			std::cout << "] : " << e.first << '\n';
		}
		if (!notSimple.empty()) {
			std::cout << "\n============Not Simple Loops============\n";
		}
		for (auto& e : notSimple) {
			auto begin = getProgramToCode(e.second);
			std::cout << std::setw(WIDTH) << e.second << " : ";
			for (auto i = begin; sourceCode[i] != ']'; ++i) {
				std::cout << sourceCode[i];
			}
			std::cout << "] : " << e.first << '\n';
		}
	}
};

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
