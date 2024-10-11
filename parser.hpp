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

enum Inst_Codes {
	NO_OP = 0,
	TAPE_M,	 // Tape Movement
	INCR,	 // Increment by product of constant and reference
	SET_C,	 // Set to constant
	WRITE,
	READ,
	JUMP_C,	 // Jump to closing bracket
	JUMP_O,	 // Jump to opening bracket
	SCAN,	 // Scan for 0
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
			return {Inst_Codes::TAPE_M, 0, +1, {}};
		case '<':
			return {Inst_Codes::TAPE_M, 0, -1, {}};
		case '+':
			return {Inst_Codes::INCR, 0, +1, {}};
		case '-':
			return {Inst_Codes::INCR, 0, -1, {}};
		case '.':
			return {Inst_Codes::WRITE, 0, 0, {}};
		case ',':
			return {Inst_Codes::READ, 0, 0, {}};
		case '[':
			return {Inst_Codes::JUMP_C, 0, 0, {}};
		case ']':
			return {Inst_Codes::JUMP_O, 0, 0, {}};
		case '$':
			return {Inst_Codes::DEBUG, 0, 0, {}};
	}
	return {Inst_Codes::NO_OP, 0, 0, {}};
}

#define LOG_INST 1

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
			break;
		case TAPE_M:
			return os << "MOV(" << a.value << ")";
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

bool mockRunner(std::span<Instruction> code, std::map<int, int>& tape) {
	int ptr = 0;
	int count = 0;
	for (auto itr = code.begin(); itr != code.end(); itr++) {
		if (count >= 512) { return false; }
		const auto& i = *itr;
		switch (i.code) {
			case TAPE_M:
				ptr += i.value;
				break;

			case SET_C:
				tape[ptr] = i.value;
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
				int t = i.value;
				for (const auto& r : i.rRef) { t *= tape[ptr + r]; }
				tape[ptr + i.lRef] += t;
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

auto solve(
	std::span<Instruction> code, const std::set<std::multiset<int>>& terms,
	const std::set<int>& variables) {
	Matrix x(0, 0);

	if (terms.size() > 20 || terms.size() <= 0) { return x; }
	const int N = static_cast<int>(terms.size()),
			  M = static_cast<int>(variables.size());

	Matrix A(N, N), b(M, N);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> d(1, 10 * N);

	std::map<int, int> tape;

	// std::vector<std::vector<int>> points = {
	// 	{1, 2, 4, 7},  //
	// 	{2, 5, 5, 4},  //
	// 	{5, 6, 1, 5},  //
	// 	{3, 3, 6, 5},  //
	// 	{6, 1, 6, 6},  //
	// 	{6, 4, 4, 1},  //
	// 	{7, 7, 4, 6}};

	for (auto i = 0; i < N; ++i) {
		// Set Tape with random numbers

		for (const auto& e : variables) {  //
			tape[e] = d(gen);
		}

		// for (int j = 0; const auto& e : variables) {  //
		// 	tape[e] = points[i][j++];
		// }

		for (int j = 0; const auto& e : terms) {
			int v = 1;
			for (const auto& f : e) { v *= tape[f]; }
			A[i][j++] = v;
		}

		if (!mockRunner(code, tape)) { return x; }
		for (int j = 0; const auto& e : variables) {  //
			b[j++][i] = tape[e];
		}
	}

	x = solve(A, b);

	// debug("A = %", A);
	// debug("b = %", b);
	// debug("x = %", x);

	return x;
}

std::vector<int> order(
	const Matrix& x, const std::set<std::multiset<int>>& terms,
	const std::set<int>& variables) {
	std::vector<std::set<int>> incoming(variables.size());
	std::vector<std::set<int>> outgoing(variables.size());
	std::vector<int> topo;
	std::set<int> S;
	std::map<int, int> ind;
	for (int i = 0; const auto& v : variables) { ind[v] = i++; }
	for (const auto& v : variables) {
		auto i = ind[v];
		for (int j = 0; const auto& t : terms) {
			if (x[i][j].get_den() != 1) { return {}; }
			for (const auto& f : t) {
				if (x[i][j] != 0 && f != v) {
					incoming[i].insert(ind[f]);
					outgoing[ind[f]].insert(i);
				}
			}
			j++;
		}
		if (incoming[i].empty()) { S.insert(i); }
	}

	while (!S.empty()) {
		auto u = *S.begin();
		S.erase(S.begin());
		topo.push_back(u);
		for (const auto& v : outgoing[u]) {
			incoming[v].erase(u);
			if (incoming[v].empty()) { S.insert(v); }
		}
	}

	for (auto& e : incoming) {
		if (!e.empty()) { return {}; }
	}
	std::reverse(topo.begin(), topo.end());
	return topo;
}

bool linearTest(
	std::span<Instruction> code, std::vector<Instruction>& newCode) {
	std::set<std::multiset<int>> terms;
	std::set<int> variables;
	auto loopBody = code.subspan(1, code.size() - 2);
	int shift = 0;
	for (auto& i : loopBody) {
		if (i.code == TAPE_M) {
			shift += i.value;
		} else if (i.code == INCR) {
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

		} else if (i.code == SET_C) {
			terms.insert({});
			terms.insert({0});
			terms.insert({i.lRef + shift});
			variables.insert(i.lRef + shift);
		} else {
			return false;
		}
	}
	if (shift != 0) { return false; }
	variables.insert(0);

	auto x = solve(loopBody, terms, variables);
	if (x.rows() == 0) { return false; }

	int loopVariable = 0;
	{  // We need to check if the loop variable (say, w) is
		// of this form w = w - 1
		// want is the matrix form of w - 1 from terms
		// loopVariable finds the index of w in variables
		for (const auto& e : variables) {
			if (e == 0) { break; }
			loopVariable++;
		}
		Matrix want(1, terms.size());
		for (int i = 0; const auto& t : terms) {
			if (t.size() == 0) {
				want[0][i] = -1;
			} else if (t.size() == 1 && *t.begin() == 0) {
				want[0][i] = 1;
			}
			i++;
		}
		// debug("want = %", want);
		if (want[0] != x[loopVariable]) { return false; }
	}

	// Finally we can solve for loop
	x = solve(code, terms, variables);

	if (x.rows() == 0) { return false; }

	// for (const auto& t : terms) {
	// 	std::cout << "c";
	// 	for (const auto& f : t) { std::cout << "*p[" << f << "]"; }
	// 	std::cout << "+";
	// }
	// std::cout << std::endl;
	// for (auto& v : variables) { std::cout << v << "\n"; }
	// Simply generating instructs from x is not enough
	// they need to be generated in proper order as we
	// are making changes in place
	auto ord = order(x, terms, variables);

	if (ord.size() == 0) { return false; }

	// return true;
	{
		std::vector<std::multiset<int>> terms_vec(terms.begin(), terms.end());
		std::vector<int> variables_vec(variables.begin(), variables.end());

		newCode.push_back({JUMP_C, 0, 0, {}});

		for (auto& i : ord) {
			auto v = variables_vec[i];
			for (auto j = 0u; j < terms.size(); ++j) {
				Instruction inst;
				inst.code = INCR;
				inst.lRef = v;
				const auto& factor = terms_vec[j];
				if (factor.size() == 1 && *factor.begin() == v) { x[i][j]--; }
				inst.value = x[i][j].get_num().get_si();
				inst.rRef.insert(inst.rRef.end(), factor.begin(), factor.end());
				if (x[i][j] != 0) { newCode.push_back(inst); }
			}
		}

		newCode.front().value = newCode.size();
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
				inst = {Inst_Codes::HALT, 0, 0, {}};
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
								const CodeInfo&, std::span<Instruction>,
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

	void optimizeSimpleLoops() {
		optimizeInnerLoops([](auto& info, auto, auto& newCode) {
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
		optimizeInnerLoops([](auto& info, auto, auto& newCode) {
			if (!isScanLoop(info)) { return false; }
			int scanJump = info.shift;
			newCode.push_back({SCAN, 0, scanJump, {}});
			return true;
		});
	}

	void linearLoops() {
		int i = 0;
		std::ofstream before("/tmp/before.bfas");
		std::ofstream after("/tmp/after.bfas");
		optimizeInnerLoops([&](auto&, auto code, auto& newCode) {
			auto x = linearTest(code, newCode);
			if (x) {
				print(before, "================%================", i);
				print(after, "================%================", i);
				for (auto& e : code) { print(before, "%", e); }
				for (auto& e : newCode) { print(after, "%", e); }
				print(before, "================%================", i);
				print(after, "================%================", i);
				i++;
				debug("PING", 0);
			}
			return x;
		});
	}
};
