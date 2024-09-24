#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using DATA_TYPE = unsigned char;

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

// #define LOG_INST 1

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

	DATA_TYPE loopCounter = 0;
	int tapePtr = 0;
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
				tapePtr += e.value;
				break;
			case INCR_C:
				if (tapePtr == 0) { loopCounter += e.value; }
				break;
		}
	}
	return loopCounter == 1 ||
		   loopCounter == std::numeric_limits<DATA_TYPE>::max();
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
		std::vector<int> stack;

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
				stack.push_back(pos);
			} else if (inst.code == JUMP_O) {
				int closing = static_cast<int>(program.size() - 1);
				if (stack.empty()) {
					err = "Mismatched loop end at char " +
						  std::to_string(getProgramToCode(closing));
					break;
				}
				int opening = stack.back();
				program[opening].value = closing - opening;
				program[closing].value = opening - closing;
				stack.pop_back();
			} else if (inst.code == HALT) {
				break;
			}
		}
		if (!stack.empty()) {
			err = "Mismatched loop start at char " +
				  std::to_string(getProgramToCode(stack.back()));
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
