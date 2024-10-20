#include <iostream>

#include "math.hpp"
#include "util.hpp"

int main() {
	Matrix a(4, 3);
	a[0] = {1, 3, -2};
	a[1] = {3, 5, 6};
	a[2] = {2, 4, 3};
	a[3] = {2, 4, 3};
	Matrix b(4, 3);
	b[0] = {5, 5, 5};
	b[1] = {7, 7, 7};
	b[2] = {8, 8, 8};
	b[3] = {8, 8, 8};
	// b[1] = {7};
	// b[2] = {8};
	// print(std::cout, "a = %", a);
	//
	debug("a = %", a);
	debug("b = %", b);
	auto x = gaussian(a, b);
	print(std::cout, "x = %", x);

	// auto x = solve(a, b);
	return 0;
}
