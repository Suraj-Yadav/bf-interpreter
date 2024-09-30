#include <immintrin.h>

#include <bitset>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>

#include "util.hpp"

using DATA_TYPE = unsigned char;

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

auto rev(auto a) {
	const auto f1 = _mm512_set_epi8(
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,  //
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,  //
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,  //
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15   //
	);
	const auto f2 = _mm512_set_epi64(1, 0, 3, 2, 5, 4, 7, 6);
	return _mm512_permutexvar_epi64(f2, _mm512_shuffle_epi8(a, f1));
}

int fastScan(const std::vector<DATA_TYPE>& tape, int BASE, int jump) {
	auto i = 0;
	const auto* ptr = (const VEC*)(&tape[BASE]);
	auto mask = maskFromJump(jump);
	const VEC v_rhs = _mm512_setzero_si512();
	for (; i + VEC_SZ <= tape.size(); i += VEC_SZ) {
		auto v_lhs = _mm512_loadu_si512(ptr);
		auto v_eq = _mm512_mask_cmpeq_epi8_mask(mask, v_lhs, v_rhs);
		if (v_eq != 0) { return std::countr_zero(v_eq) + i; }
		ptr++;
	}
	return -1;
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
	int shift = 0, mask_mask = 0;

	if constexpr (!isPowerOf2) {
		// Below 2 are only used when powerOf2 is false;
		// how much the mask shifts
		shift = jump - static_cast<int>(VEC_SZ) % jump;
		// used to update mask
		mask_mask = (1 << (shift + 1)) - 1;

		// std::cout << "shift = " << shift << "\n";
	}

	// value to test for
	const VEC v_rhs = _mm512_setzero_si512();

	for (;; i += VEC_SZ) {
		// load VEC_SZ elements into a variable
		auto v_lhs = _mm512_loadu_si512(ptr);
		if constexpr (isJumpNegative) { v_lhs = rev(v_lhs); }
		// result has its ith bit set if ith element == 0
		auto v_eq = _mm512_mask_cmpeq_epi8_mask(mask, v_lhs, v_rhs);

		// std::cout << std::setw(3) << i << " = " <<
		// std::bitset<BATCH_SZ>(mask)
		// 		  << "\n";
		// std::cout << "      " << std::bitset<BATCH_SZ>(v_eq) << "\n";

		if (v_eq != 0) {						// found something somewhere
			return std::countr_zero(v_eq) + i;	// find the first set bit
		}

		if constexpr (!isPowerOf2) {
			mask = (mask << shift) | (mask_mask & (mask >> (jump - shift)));
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

int main() {
	std::random_device rd;
	std::mt19937 g(rd());
	std::uniform_int_distribution<> d(100, 600);

	auto start = std::chrono::high_resolution_clock::now();
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> diff = end - start;

	double stime = 0, ftime = 0;

	int a = 0, b = 0;

	const auto REPEAT = 1000000;
	const auto SIZE = 700;
	const auto LOOP = 7;

	auto n = d(g);
	auto j = d(g) / 100;
	j = 1;
	n -= n % j;
	// n = 261;
	// j = 3;
	for (auto i = 0; i < REPEAT; ++i) {
		const auto BASE = SIZE - 1;
		std::vector<DATA_TYPE> h(2 * SIZE - 1, 0);

		for (auto i = 0; i < SIZE; ++i) {
			if (i % j == 0) {
				h[BASE + i] = i % LOOP + 1;
			} else {
				h[BASE + i] = 0;
			}
		}
		for (auto i = 0; i < SIZE; ++i) {
			if (i % j == 0) {
				h[BASE - i] = i % LOOP + 1;
			} else {
				h[BASE - i] = 0;
			}
		}
		h[BASE - n] = 0;

		start = std::chrono::high_resolution_clock::now();
		a = slowScan(h, BASE, -j);
		end = std::chrono::high_resolution_clock::now();
		diff = end - start;
		stime += diff.count();

		start = std::chrono::high_resolution_clock::now();
		b = scan(h, BASE, -j);
		end = std::chrono::high_resolution_clock::now();
		diff = end - start;
		ftime += diff.count();

		if (a == b) { continue; }

		print(std::cout, "n: %, j: %", n, j);
		print(std::cout, "FAST FAIL: EXPECTED: % GOT: %", a, b);
		for (auto i = 0; i < 2 * SIZE - 1;) {
			if (i == (BASE + a)) { std::cout << "\x1B[32m"; }
			if (i == (BASE + b)) { std::cout << "\x1B[31m"; }
			if (i % j == 0) {
				std::cout << "*";
			} else {
				std::cout << " ";
			}
			std::cout << std::setw(4) << i - BASE << "=" << (int)h[i] << ",";
			if (i == (BASE + a)) { std::cout << "\033[0m"; }
			if (i == (BASE + b)) { std::cout << "\033[0m"; }
			i++;
			if (i % 16 == 0) { std::cout << '\n'; }
		}
		std::cout << "\n";
		break;
	}

	print(
		std::cout,
		"Time a = %, j = %, n = %, slow: %s\tfast: %s\nSLOW/FAST = %", a, j, n,
		stime, ftime, stime / ftime);

	return 0;
}
