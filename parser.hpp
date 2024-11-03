#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "math.hpp"
#include "util.hpp"

using DATA_TYPE = unsigned char;

enum Inst_Codes : std::int8_t {
	NO_OP = 0,
	TAPE_M,	 // Tape Movement
	INCR,	 // Increment by product of constant and reference
	SET_C,	 // Set to constant
	WRITE,
	READ,
	JUMP_C,		   // Jump to closing bracket
	JUMP_O,		   // Jump to opening bracket
	SCAN,		   // Scan for 0
	WRITE_LOCK,	   // stop writes to cell, storing writes in a temp cell
	WRITE_UNLOCK,  // set value from temp cell and enable writes to cell
	DEBUG,
	HALT,
};

struct Instruction {
	Inst_Codes code = Inst_Codes::NO_OP;
	int lRef = 0;
	int value = 0;
	std::vector<int> rRef;
};

Instruction getInstruction(char ch) {
	switch (ch) {
		case '>':
			return {
				.code = Inst_Codes::TAPE_M, .lRef = 0, .value = +1, .rRef = {}};
		case '<':
			return {
				.code = Inst_Codes::TAPE_M, .lRef = 0, .value = -1, .rRef = {}};
		case '+':
			return {
				.code = Inst_Codes::INCR, .lRef = 0, .value = +1, .rRef = {}};
		case '-':
			return {
				.code = Inst_Codes::INCR, .lRef = 0, .value = -1, .rRef = {}};
		case '.':
			return {
				.code = Inst_Codes::WRITE, .lRef = 0, .value = 0, .rRef = {}};
		case ',':
			return {
				.code = Inst_Codes::READ, .lRef = 0, .value = 0, .rRef = {}};
		case '[':
			return {
				.code = Inst_Codes::JUMP_C, .lRef = 0, .value = 0, .rRef = {}};
		case ']':
			return {
				.code = Inst_Codes::JUMP_O, .lRef = 0, .value = 0, .rRef = {}};
		case '$':
			return {
				.code = Inst_Codes::DEBUG, .lRef = 0, .value = 0, .rRef = {}};
	}
	return {.code = Inst_Codes::NO_OP, .lRef = 0, .value = 0, .rRef = {}};
}

// #define LOG_INST 1

std::ostream& operator<<(std::ostream& os, const Instruction& a) {
	switch (a.code) {
		case INCR:
			os << "INCR(p[" << a.lRef << "]+=" << a.value;
			for (const auto& e : a.rRef) { os << "*p[" << e << "]"; }
			return os << ")";
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
		case TAPE_M:
			return os << "MOV(" << a.value << ")";
		case WRITE_LOCK:
			return os << "W_LOCK(" << a.lRef << ")";
		case WRITE_UNLOCK:
			return os << "W_UNLOCK(" << a.lRef << ")";
			break;
	}
	return os;
}

struct CodeInfo {
	bool loop = false, innerMost = false, complex = false, empty = true,
		 hasJumps = false;
	int shift = 0;
	std::map<int, int> delta;
	std::map<int, std::set<int>> parent;
};

CodeInfo codeInfo(std::span<Instruction> code) {
	if (code.empty()) { return {}; }
	CodeInfo info;
	auto& delta = info.delta;
	auto& shift = info.shift;
	delta.clear();

	for (auto& e : code) {
		switch (e.code) {
			case SCAN:
			case WRITE:
			case READ:
			case DEBUG:
			case HALT:
				info.complex = true;
				break;
			case TAPE_M:
				shift += e.value;
				break;
			case INCR:
				if (e.rRef.empty()) {
					delta[info.shift] += e.value;
				} else {
					for (auto& r : e.rRef) {
						info.parent[info.shift + e.lRef].insert(info.shift + r);
					}
				}
				break;
			case SET_C:
				delta.erase(info.shift + e.lRef);
				info.parent[info.shift + e.lRef].insert(info.shift + e.lRef);
				break;
			case JUMP_C:
			case JUMP_O:
				info.hasJumps = true;
				break;
			case NO_OP:
			case WRITE_LOCK:
			case WRITE_UNLOCK:
				break;
		}
	}
	return info;
}

CodeInfo loopInfo(std::span<Instruction> code) {
	CodeInfo info;
	if (code.empty()) { return info; }
	if (code.front().code != JUMP_C) { return info; }
	if (code.back().code != JUMP_O) { return info; }
	code = code.subspan(1, code.size() - 2);
	info = codeInfo(code);
	info.loop = true;
	info.innerMost = !info.hasJumps;
	return info;
}

bool isInnerMostLoop(const CodeInfo& info) {
	return info.loop && info.innerMost;
}

bool isSimpleLoop(const CodeInfo& info) {
	return info.loop && !info.complex && info.shift == 0 &&	 //
		   info.parent.empty() &&							 //
		   info.delta.find(0) != info.delta.end() &&		 //
		   info.delta.at(0) == -1;
}

bool isScanLoop(const CodeInfo& info) {
	return info.loop && !info.complex && info.shift != 0 &&	 //
		   info.delta.empty() &&							 //
		   info.parent.empty();
}

bool mockRunner(std::span<Instruction> code, std::map<int, mpz_class>& tape) {
	int ptr = 0;
	int count = 0;
	std::map<int, mpz_class> tempTape;
	constexpr auto LOOP_LIMIT = 512;
	for (auto itr = code.begin(); itr != code.end(); itr++) {
		if (count >= LOOP_LIMIT) { return false; }
		const auto& i = *itr;
		switch (i.code) {
			case TAPE_M:
				ptr += i.value;
				break;

			case WRITE_LOCK:
				tempTape[ptr + i.lRef] = tape[ptr + i.lRef];
				break;
			case WRITE_UNLOCK:
				tape[ptr + i.lRef] = tempTape[ptr + i.lRef];
				tempTape.erase(ptr + i.lRef);
				break;

			case SET_C:
				if (tempTape.contains(ptr + i.lRef)) {
					tempTape[ptr + i.lRef] = i.value;
				} else {
					tape[ptr + i.lRef] = i.value;
				}
				break;

			case JUMP_C:
				if (tape[ptr] == 0) { itr += i.value; }
				break;

			case JUMP_O:
				if (tape[ptr] != 0) {
					itr += i.value;
					count++;
				}
				break;

			case INCR: {
				mpz_class t = i.value;
				for (const auto& r : i.rRef) { t *= tape[ptr + r]; }
				if (tempTape.contains(ptr + i.lRef)) {
					tempTape[ptr + i.lRef] += t;
				} else {
					tape[ptr + i.lRef] += t;
				}
				break;
			}

			case NO_OP:
				break;
			case SCAN:
			case WRITE:
			case READ:
			case DEBUG:
			case HALT:
				return false;
		}
	}
	return true;
}

constexpr auto VARIABLE_LIMIT = 20u;

auto solve(
	std::span<Instruction> code, const std::set<std::multiset<int>>& terms,
	const std::set<int>& variables) {
	Matrix x(0, 0);

	if (terms.size() > VARIABLE_LIMIT || terms.empty()) { return x; }
	const int N = static_cast<int>(terms.size()),
			  M = static_cast<int>(variables.size()), S = N + 1;

	// S = # samples
	// N = # coeffs to solve for
	// M = # distinct linear systems to solve. one for each variable

	// Matrix for solving Ax = b
	Matrix A(S, N), b(S, M);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> d(1, N * N);

	std::map<int, mpz_class> tape;

	// std::vector<std::vector<int>> points = {
	// };

	GaussianResult res = MANY_SOLUTIONS;

	while (res == MANY_SOLUTIONS) {
		for (auto i = 0; i < S; ++i) {
			// Set Tape with random numbers
			for (const auto& e : variables) {  //
				tape[e] = d(gen);
			}

			// for (int j = 0; const auto& e : variables) {  //
			// 	tape[e] = points[i][j++];
			// }

			for (int j = 0; const auto& e : terms) {
				A[i][j] = 1;
				for (const auto& f : e) { A[i][j] *= tape[f]; }
				j++;
			}

			if (!mockRunner(code, tape)) { return x; }
			for (int j = 0; const auto& e : variables) {  //
				b[i][j++] = tape[e];
			}
		}

		std::tie(res, x) = gaussian(A, b);
	}
	// debug("A = %", A);
	// debug("b = %", b);
	// debug("x = %", x);

	// its easier to process transposed x
	return x.T();
}

bool extractVariables(
	std::span<Instruction> code, std::set<std::multiset<int>>& terms,
	std::set<int>& variables) {
	code = code.subspan(1, code.size() - 2);
	if (code.empty()) { return false; }
	int shift = 0;
	for (auto& i : code) {
		switch (i.code) {
			case TAPE_M: {
				shift += i.value;
				break;
			}
			case INCR: {
				std::multiset<int> term;
				for (auto& e : i.rRef) {
					term.insert(e + shift);
					variables.insert(e + shift);
				}
				terms.insert(term);
				term.insert(0);
				terms.insert(term);
				terms.insert({i.lRef + shift});
				variables.insert(i.lRef + shift);
				break;
			}
			case SET_C: {
				terms.insert({});
				terms.insert({0});
				terms.insert({i.lRef + shift});
				variables.insert(i.lRef + shift);
				break;
			}
			case WRITE_LOCK:
			case WRITE_UNLOCK:
			case NO_OP:
				break;
			case WRITE:
			case READ:
			case JUMP_C:
			case JUMP_O:
			case SCAN:
			case DEBUG:
			case HALT:
				return false;
		}
	}
	if (terms.empty()) { return false; }
	variables.insert(0);
	{
		auto degree = terms.begin()->size();
		for (const auto& e : terms) { degree = std::max(degree, e.size()); }
		std::multiset<int> term;
		for (auto i = 0u; i < degree; ++i) {
			term.insert(0);
			terms.insert(term);
		}
	}
	return shift == 0;
}

auto rowToExpr(const std::set<std::multiset<int>>& terms, auto& row) {
	std::map<std::multiset<int>, mpq_class> expr;
	for (int i = 0; const auto& e : terms) {
		if (row[i] != 0) { expr[e] = row[i]; }
		i++;
	}
	return expr;
}

// check if the loop body makes this change
// w = w - 1
bool checkLoopBody(
	std::span<Instruction> code, const std::set<std::multiset<int>>& terms,
	const std::set<int>& variables) {
	auto loopBody = code.subspan(1, code.size() - 2);

	auto x = solve(loopBody, terms, variables);
	if (x.rows() == 0) { return false; }
	int loopVariable = 0;
	for (const auto& e : variables) {
		if (e == 0) { break; }
		loopVariable++;
	}
	std::map<std::multiset<int>, mpq_class> want;
	want[{}] = -1;
	want[{0}] = 1;
	// want = p[0] - 1;
	return rowToExpr(terms, x[loopVariable]) == want;
}

auto computeExpressions(
	const Matrix& x, const std::set<std::multiset<int>>& terms,
	const std::set<int>& variables) {
	std::vector<std::map<std::multiset<int>, mpq_class>> expressions(
		variables.size());
	for (auto i = 0u; const auto& v : variables) {
		expressions[i] = rowToExpr(terms, x[i]);
		// We can only increment tape cells, so subtract cell from it
		expressions[i][{v}]--;
		if (expressions[i][{v}] == 0) {	 //
			expressions[i].erase({v});
		}
		i++;
	}
	return expressions;
}

bool linearTest(
	std::span<Instruction> code, std::vector<Instruction>& newCode) {
	std::set<std::multiset<int>> terms;
	std::set<int> variables;

	if (!extractVariables(code, terms, variables)) { return false; }

	if (!checkLoopBody(code, terms, variables)) { return false; }

	// Finally solve for loop
	auto x = solve(code, terms, variables);

	if (x.rows() == 0) { return false; }

	// The matrix can have fractional elements,
	// but we can't handle them for now, so return false
	// Also, matrix elements has arbitrary precision integer
	// so need to reject elements out of range
	for (auto i = 0u; i < x.rows(); ++i) {
		for (auto j = 0u; j < x.cols(); ++j) {
			if (x[i][j].get_den() != 1) { return false; }
			if (x[i][j] > INT_MAX || x[i][j] < INT_MIN) { return false; }
		}
	}

	newCode.push_back({JUMP_C, 0, 0, {}});

	for (const auto& v : variables) {
		newCode.push_back({WRITE_LOCK, v, 0, {}});
	}

	auto expressions = computeExpressions(x, terms, variables);
	bool canSkipCheck = true;

	for (auto i = 0u; const auto& v : variables) {
		auto& expr = expressions[i++];
		if (expr.size() == 1 && expr.contains({v}) && expr[{v}] == -1) {
			canSkipCheck = canSkipCheck && v == 0;
			Instruction inst;
			inst.code = SET_C;
			inst.lRef = v;
			inst.value = 0;
			newCode.push_back(inst);
			continue;
		}
		for (auto& [term, coeff] : expr) {
			canSkipCheck = canSkipCheck && term.contains(0);
			Instruction inst;
			inst.code = INCR;
			inst.lRef = v;
			inst.value = static_cast<int>(coeff.get_num().get_si());
			inst.rRef.insert(inst.rRef.end(), term.begin(), term.end());
			newCode.push_back(inst);
		}
	}

	for (const auto& v : variables) {
		newCode.push_back({WRITE_UNLOCK, v, 0, {}});
	}

	if (canSkipCheck) {
		newCode.erase(newCode.begin());
	} else {
		newCode.front().value = static_cast<int>(newCode.size());
		newCode.push_back({JUMP_O, -1, -newCode.front().value, {}});
	}

	return true;
}

class Program {
	std::optional<std::string> err;
	std::vector<Instruction> program;
	std::vector<int> srcToProgram;

	void aggregate() {
		if (program.size() < 2) { return; }
		auto& b = program.back();
		auto& a = program[program.size() - 2];
		if (a.code == b.code && a.code == Inst_Codes::INCR &&
			a.lRef == b.lRef && a.rRef.empty() && b.rRef.empty()) {
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
			std::ranges::lower_bound(srcToProgram, pos) - srcToProgram.begin());
	}

	void parse(const Args& args) {
		if (args.input.empty()) {
			err = "fatal error: no input files";
			return;
		}
		std::ifstream input(args.input);
		if (!input.is_open()) {
			err = "cannot read file: '" + args.input.string() + "'";
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
				inst = {
					.code = Inst_Codes::HALT,
					.lRef = 0,
					.value = 0,
					.rRef = {}};
			}

			if (inst.code != NO_OP) {
				program.push_back(inst);
#ifdef LOG_INST
				original << inst << "\n";
#endif
				aggregate();
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
		std::ranges::sort(loops, std::greater<>());

		const auto H_BAR = 80u;

		if (!loops.empty()) {
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

	void optimizeInnerLoops(
		const std::string& name, const std::function<bool(
									 const CodeInfo&, std::span<Instruction>,
									 std::vector<Instruction>&)>& optimizer) {
		std::vector<Instruction> p, newCode;
		std::vector<int> stack;

#ifdef LOG_INST
		int count = 0;
		std::ofstream before(std::string("/tmp/before-") + name + ".bfas");
		std::ofstream after(std::string("/tmp/after-") + name + ".bfas");
#endif

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
#ifdef LOG_INST
				for (auto& e : code) { print(before, "%", e); }
				print(before, "================%================", count);
				for (auto& e : code) { print(before, "%", e); }
				print(before, "================%================", count);
				for (auto& e : code) { print(after, "%", e); }
				print(after, "================%================", count);
				for (auto& e : newCode) { print(after, "%", e); }
				print(after, "================%================", count);
				count++;
#endif
				p.erase(begin, end);
				p.insert(p.end(), newCode.begin(), newCode.end());
				newCode.clear();
			}
		}

		program = p;

#ifdef LOG_INST
		print(std::cerr, "Optimizations by %: %", name, count);
		std::ofstream optimized("/tmp/actual.bfas");
		for (const auto& i : program) { optimized << i << "\n"; }
#endif
	}
	void optimizeSimpleLoops() {
		optimizeInnerLoops(__FUNCTION__, [](auto& info, auto, auto& newCode) {
			if (!isSimpleLoop(info)) { return false; }
			auto delta = info.delta;
			int change = -delta[0];
			delta.erase(0);
			newCode.reserve(delta.size());
			for (auto& e : delta) {
				newCode.push_back({INCR, e.first, change * e.second, {0}});
			}
			newCode.push_back({SET_C, 0, 0, {}});
			return true;
		});
	}

	void optimizeScans() {
		optimizeInnerLoops(__FUNCTION__, [](auto& info, auto, auto& newCode) {
			if (!isScanLoop(info)) { return false; }
			int scanJump = info.shift;
			newCode.push_back({SCAN, 0, scanJump, {}});
			return true;
		});
	}

	void linearizeLoops() {
		optimizeInnerLoops(__FUNCTION__, [&](auto&, auto code, auto& newCode) {
			return linearTest(code, newCode);
		});
	}

   public:
	[[nodiscard]] auto isOK() const { return !err.has_value(); }

	Program(const Args& args) {
		parse(args);
		if (isOK()) {
			if (args.optimizeSimpleLoops) { optimizeSimpleLoops(); }
			if (args.optimizeScans) { optimizeScans(); }
			if (args.linearizeLoops) { linearizeLoops(); }
		}
#ifdef LOG_INST
		std::ofstream optimized("/tmp/actual.bfas");
		for (const auto& i : program) { optimized << i << "\n"; }
#endif
	}
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

			// if (!isInnerMostLoop(info)) { continue; }
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
};
