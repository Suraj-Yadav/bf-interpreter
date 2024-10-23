#include <immintrin.h>

#include <iostream>
#include <span>
#include <vector>

#include "parser.hpp"
#include "util.hpp"

int slowScan(const std::vector<DATA_TYPE>& tape, int BASE, int jump) {
	for (auto i = 0;; i += jump) {
		if (tape[i + BASE] == 0) { return i; }
	}
	return -1;
}

using VEC = __m512i;
constexpr auto VEC_SZ = sizeof(VEC) / sizeof(DATA_TYPE);

auto maskFromJump(int jump) {
	__mmask64 m = 0;
	for (auto i = 0u; i < VEC_SZ; i += jump) { m = m | 1ULL << i; }
	return m;
}

template <bool isPowerOf2, bool isJumpNegative>
int fastScan(const std::vector<DATA_TYPE>& tape, int BASE, int jump) {
	auto i = 0;
	const auto* ptr = (const VEC*)(&tape[BASE]);

	if constexpr (isJumpNegative) {
		ptr = (const VEC*)(&tape[BASE - VEC_SZ + 1]);
	}

	// generate a mask marking elements visited by jump
	auto mask = maskFromJump(jump);
	int shift = 0;

	if constexpr (!isPowerOf2) {
		// only used when powerOf2 is false;
		// how much the mask shifts
		shift = jump - static_cast<int>(VEC_SZ) % jump;
		// std::cout << "shift = " << shift << "\n";
	}
	if (isJumpNegative) { mask = revBits(mask); }

	// value to test for
	const VEC v_rhs = _mm512_setzero_si512();

	for (;; i += VEC_SZ) {
		// load VEC_SZ elements into a variable
		auto v_lhs = _mm512_loadu_si512(ptr);
		// result has its ith bit set if ith element == 0
		auto v_eq = _mm512_mask_cmpeq_epi8_mask(mask, v_lhs, v_rhs);

		if (v_eq != 0) {  // found something somewhere
			// find the first/last set bit
			if constexpr (isJumpNegative) { return std::countl_zero(v_eq) + i; }
			return std::countr_zero(v_eq) + i;
		}

		if constexpr (!isPowerOf2) {
			if constexpr (isJumpNegative) {
				mask = (mask >> shift) | (mask << (jump - shift));
			} else {
				mask = (mask << shift) | (mask >> (jump - shift));
			}
		}
		if constexpr (isJumpNegative) {
			ptr--;
		} else {
			ptr++;
		}
	}
	return -1;
}

int scan(const std::vector<DATA_TYPE>& tape, int BASE, int jump) {
	if (jump == 0) {
		if (tape[BASE] == 0) { return 0; }
	}
	if (jump <= -16 || jump >= 16) { return slowScan(tape, BASE, jump); }
	auto isNeg = jump < 0;
	if (isNeg) { jump = -jump; }
	auto isPowerOf2 = (jump & (jump - 1)) == 0;

	if (isPowerOf2) {
		if (isNeg) { return -fastScan<true, true>(tape, BASE, jump); }
		return fastScan<true, false>(tape, BASE, jump);
	}
	if (isNeg) { return -fastScan<false, true>(tape, BASE, jump); }
	return fastScan<false, false>(tape, BASE, jump);
}

auto run(std::span<Instruction> code) {
	const auto TAPE_LENGTH = 1000000u;

	std::vector<DATA_TYPE> tape(TAPE_LENGTH, 0);
	int ptr = TAPE_LENGTH / 2;

	std::map<int, DATA_TYPE> temp;

	std::vector<int> count(code.size(), 0);
	for (auto itr = code.begin(); itr != code.end(); itr++) {
		const auto& inst = *itr;
		count[itr - code.begin()]++;
		switch (inst.code) {
			case TAPE_M:
				ptr += inst.value;
				break;

			case SCAN: {
				ptr += scan(tape, ptr, inst.value);
				break;
			}

			case WRITE_LOCK:
				temp[ptr + inst.lRef] = tape[ptr + inst.lRef];
				break;

			case WRITE_UNLOCK:
				tape[ptr + inst.lRef] = temp[ptr + inst.lRef];
				temp.erase(ptr + inst.lRef);
				break;

			case SET_C:
				if (temp.contains(ptr + inst.lRef)) {
					temp[ptr + inst.lRef] = inst.value;
				} else {
					tape[ptr + inst.lRef] = inst.value;
				}
				break;

			case WRITE:
				std::putchar(tape[ptr]);
				break;

			case READ:
				tape[ptr] = std::getchar();
				break;

			case JUMP_C:
				if (tape[ptr] == 0) { itr += inst.value; }
				break;

			case JUMP_O:
				if (tape[ptr] != 0) { itr += inst.value; }
				break;

			case INCR: {
				DATA_TYPE t = inst.value;
				for (const auto& r : inst.rRef) { t *= tape[ptr + r]; }
				if (temp.contains(ptr + inst.lRef)) {
					temp[ptr + inst.lRef] += t;
				} else {
					tape[ptr + inst.lRef] += t;
				}
				break;
			}

			case DEBUG: {
				std::cout << "tape[" << ptr << "] = " << (int)tape[ptr]
						  << std::endl;
				// 		std::cout << "index = " << index << '\n';
				// 		while (!left.empty()) {
				// 			const auto& e = left.back();
				// 			std::cout << (int)e << "\t";
				// 			left.pop_back();
				// 		}
				// 		for (auto& e : right) { std::cout << (int)e << "\t"; }
				// 		std::cout << '\n';
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
	auto args = argparse(argc, argv);

	if (args.input.empty()) {
		std::cerr << "bf: fatal error: no input files\n";
		return 1;
	}

	Program p(args.input);

	if (!p.isOK()) {
		std::cerr << p.error() << "\n";
		return 1;
	}

	if (args.optimizeSimpleLoops) { p.optimizeSimpleLoops(); }
	if (args.optimizeScans) { p.optimizeScans(); }
	if (args.optimizeSecondLevelLoops) { p.linearLoops(); }

	auto& code = p.instructions();

	if (args.profile) {
		auto counts = run(code);
		p.printProfileInfo(counts);
	} else {
		run(code);
	}

	return 0;
}
