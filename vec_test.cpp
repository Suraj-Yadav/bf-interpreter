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

auto maskToString(auto mask) {
	auto str = std::bitset<VEC_SZ>(mask).to_string();
	std::reverse(str.begin(), str.end());
	return str;
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
	auto mask_mask = 0 * mask;
	int shift = 0;

	if constexpr (!isPowerOf2) {
		// Below 2 are only used when powerOf2 is false;
		// how much the mask shifts
		shift = jump - static_cast<int>(VEC_SZ) % jump;
		// used to update mask
		mask_mask = (1 << (shift + 1)) - 1;

		// std::cout << "shift = " << shift << "\n";
	}
	if (isJumpNegative) {
		mask = revBits(mask);
		mask_mask = revBits(mask_mask);
	}

	// value to test for
	const VEC v_rhs = _mm512_setzero_si512();

	for (;; i += VEC_SZ) {
		// load VEC_SZ elements into a variable
		auto v_lhs = _mm512_loadu_si512(ptr);
		// result has its ith bit set if ith element == 0
		auto v_eq = _mm512_mask_cmpeq_epi8_mask(mask, v_lhs, v_rhs);

		// std::cout << std::setw(3) << i << " = " << maskToString(mask) <<
		// "\n"; std::cout << std::setw(3) << i << " = " <<
		// maskToString(mask_mask)
		// << "\n";
		// std::cout << "      " << maskToString(v_eq) << "\n";

		if (v_eq != 0) {  // found something somewhere
			// find the first/last set bit
			if constexpr (isJumpNegative) { return std::countl_zero(v_eq) + i; }
			return std::countr_zero(v_eq) + i;
		}

		if constexpr (!isPowerOf2) {
			if constexpr (isJumpNegative) {
				mask = (mask >> shift) | (mask_mask & (mask << (jump - shift)));
			} else {
				mask = (mask << shift) | (mask_mask & (mask >> (jump - shift)));
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
	n -= n % j;
	// j = 3;
	// n = 267;
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
