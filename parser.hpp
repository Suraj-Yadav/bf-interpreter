#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
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
	SET_C,	 // Set to constant
	SCAN,	 // Scan for 0
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

std::ostream& operator<<(std::ostream& os, const Instruction& a) {
	switch (a.code) {
		case INCR_R:
			return os << "INCR(p[" << a.lRef << "]+=" << a.value << "*p["
					  << a.rRef << "])";
		case WRITE:
			return os << "WRITE";
		case READ:
			return os << "READ";
		case DEBUG:
			return os << "DEBUG";
		case SCAN:
			return os << "SCAN(" << a.value << ")";
		case HALT:
			return os << "HALT";
		case SET_C:
			return os << "SET(p[" << a.lRef << "]=" << a.value << ")";
		case JUMP_C:
			return os << "JUMP(" << a.value << ")";
		case JUMP_O:
			return os << "JUMP(" << a.value << ")";
		case NO_OP:
			return os << "NO_OP";
			break;
		case TAPE_M:
			return os << "MOV(" << a.value << ")";
			break;
		case INCR_C:
			return os << "INCR(p[" << a.lRef << "]+=" << a.value << ")";
			break;
	}
	return os;
}

struct LoopInfo {
	bool loop = false, innerMost = false, io = false;
	int shift = 0;
	std::map<int, int> delta;
	std::map<int, std::set<int>> parent;
};

LoopInfo loopInfo(std::span<Instruction> code) {
	LoopInfo info;
	if (code.empty()) { return info; }
	if (code.front().code != JUMP_C) { return info; }
	if (code.back().code != JUMP_O) { return info; }
	code = code.subspan(1, code.size() - 2);
	if (code.empty()) { return info; }
	info.loop = true;

	auto& delta = info.delta;
	auto& shift = info.shift;
	delta.clear();

	for (auto& e : code) {
		switch (e.code) {
			case JUMP_C:
			case JUMP_O:
			case SCAN:
			case HALT:
				return info;
			case WRITE:
			case READ:
			case DEBUG:
				info.io = true;
				return info;
			case TAPE_M:
				shift += e.value;
				break;
			case INCR_C:
				delta[info.shift] += e.value;
				break;
			case INCR_R:
				info.parent[info.shift + e.lRef].insert(info.shift + e.rRef);
				break;
			case SET_C:
				delta.erase(info.shift + e.lRef);
				info.parent[info.shift + e.lRef].insert(info.shift + e.lRef);
				break;
			case NO_OP:
				break;
		}
	}
	info.innerMost = true;
	return info;
}

bool isInnerMostLoop(const LoopInfo& info) {
	return info.loop && info.innerMost;
}

bool isSimpleLoop(const LoopInfo& info) {
	return info.loop && !info.io && info.shift == 0 &&	//
		   info.parent.empty() &&						//
		   info.delta.find(0) != info.delta.end() &&	//
		   info.delta.at(0) == -1;
}

bool isScanLoop(const LoopInfo& info) {
	return info.loop && !info.io && info.shift != 0 &&	//
		   info.delta.empty() &&						//
		   info.parent.empty();
}

class Program {
	std::optional<std::string> err;
	std::vector<Instruction> program;
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

	void printLoops(
		const std::string& title, std::vector<std::pair<int, int>>& loops) {
		std::sort(loops.begin(), loops.end(), std::greater<>());

		const auto H_BAR = 80u;

		if (loops.size() > 0) {
			auto left = (H_BAR - title.size()) / 2;
			auto right = H_BAR - left - title.size();
			std::cout << '\n'
					  << std::string(left, '=') << title
					  << std::string(right, '=') << '\n';
		}
		for (const auto& loop : loops) {
			auto start = loop.second;
			auto count = loop.first;
			auto end = program[start].value + start + 1;

			constexpr auto WIDTH = 5;

			std::cout << std::setw(WIDTH) << start << " : ";

			for (auto i = start; i < end; ++i) {
				std::cout << program[i] << ',';
			}
			std::cout << " : " << count << "\n";
		}
	}

	void optimizeInnerLoops(const std::function<bool(
								const LoopInfo&, std::span<Instruction>,
								std::vector<Instruction>&)>& optimizer) {
		std::vector<Instruction> p, newCode;
		std::vector<int> stack;

		for (auto& inst : program) {
			p.push_back(inst);
			if (inst.code == JUMP_C) {
				stack.push_back(static_cast<int>(p.size() - 1));
			}
			if (inst.code != JUMP_O) { continue; }

			// compute the new jump deltas
			int closing = static_cast<int>(p.size() - 1);
			int opening = stack.back();
			p[closing].value = opening - closing;
			p[opening].value = closing - opening;
			stack.pop_back();

			auto begin = p.end() + p.back().value - 1;
			auto end = p.end();
			std::span<Instruction> code(begin, end);
			auto info = loopInfo(code);
			if (isInnerMostLoop(info) && optimizer(info, code, newCode)) {
				p.erase(begin, end);
				p.insert(p.end(), newCode.begin(), newCode.end());
				newCode.clear();
			}
		}

		program = p;

#ifdef LOG_INST
		std::ofstream optimized("/tmp/actual.bfas");
		for (const auto& i : program) { optimized << i << "\n"; }
#endif
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
		for (auto i = 0u; i < program.size(); ++i) {
			std::cout << std::setw(WIDTH) << i << " : " << program[i] << " : "
					  << runCounts[i] << "\n";
		}
		std::vector<std::pair<int, int>> simple, notSimple, scan;
		for (auto i = 0u; i < program.size(); ++i) {
			const auto& instr = program[i];
			if (instr.code != JUMP_C) { continue; }
			const auto runCount = runCounts[i + instr.value];

			std::span<Instruction> code(
				program.begin() + i, program.begin() + i + instr.value + 1);

			auto info = loopInfo(code);

			if (!isInnerMostLoop(info)) { continue; }
			if (isSimpleLoop(info)) {
				simple.emplace_back(runCount, i);
			} else if (isScanLoop(info)) {
				scan.emplace_back(runCount, i);
			} else {
				notSimple.emplace_back(runCount, i);
			}
		}

		printLoops("Simple Loops", simple);
		printLoops("Scan Loops", scan);
		printLoops("Non Simple Loops", notSimple);
	}

	void optimizeSimpleLoops() {
		optimizeInnerLoops([](auto& info, auto, auto& newCode) {
			if (!isSimpleLoop(info)) { return false; }
			auto delta = info.delta;
			int change = -delta[0];
			delta.erase(0);
			newCode.reserve(delta.size());
			for (auto& e : delta) {
				newCode.push_back({INCR_R, e.first, 0, change * e.second});
			}
			newCode.push_back({SET_C, 0, 0, 0});
			return true;
		});
	}

	void optimizeScans() {
		optimizeInnerLoops([](auto& info, auto, auto& newCode) {
			if (!isScanLoop(info)) { return false; }
			int scanJump = info.shift;
			newCode.push_back({SCAN, 0, 0, scanJump});
			return true;
		});
	}
};
