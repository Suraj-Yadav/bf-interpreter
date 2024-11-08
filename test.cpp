#include <iostream>

#include "math.hpp"
#include "util.hpp"

int main() {
	Matrix a(8, 8);
	auto i = 0;
	a[i++] = {1, 7, 42, 630, 50, 750, 15, 225};
	a[i++] = {1, 34, 32, 128, 64, 256, 4, 16};
	a[i++] = {1, 37, 63, 3402, 63, 3402, 54, 2916};
	a[i++] = {1, 29, 24, 312, 3, 39, 13, 169};
	a[i++] = {1, 2, 23, 414, 36, 648, 18, 324};
	a[i++] = {1, 7, 42, 2142, 18, 918, 51, 2601};
	a[i++] = {1, 40, 32, 1024, 62, 1984, 32, 1024};
	a[i++] = {1, 8, 63, 2583, 6, 246, 41, 1681};
	// a[i++] = {1, 45, 49, 2499, 36, 1836, 51, 2601};
	Matrix b(8, 4);
	i = 0;
	b[i++] = {64, 0, -7, 0};
	b[i++] = {68, 0, 32, 0};
	b[i++] = {127, 0, 0, 0};
	b[i++] = {38, 0, 22, 0};
	b[i++] = {34, 0, 13, 0};
	b[i++] = {50, 0, 25, 0};
	b[i++] = {88, 0, 30, 0};
	b[i++] = {34, 0, 58, 0};
	// b[i++] = {106, 0, 14, 0};
	debug("a = %", a);
	debug("b = %", b);
	auto x = gaussian(a, b);
	print(std::cout, "x = %", x.first);

	// auto x = solve(a, b);
	return 0;
}
